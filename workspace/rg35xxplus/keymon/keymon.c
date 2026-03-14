#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>

#include <msettings.h>
#include "power.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <poll.h>

// #include "defines.h"

#define VOLUME_MIN 		0
#define VOLUME_MAX 		20
#define BRIGHTNESS_MIN 	0
#define BRIGHTNESS_MAX 	10

// evdev key codes (event1 — face/system buttons)
#define CODE_MENU		312 // but also 354
#define CODE_PLUS		115
#define CODE_MINUS		114

// evdev key code for power button (event0)
#define CODE_POWER		116

//	for ev.value
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

#define SLEEP_DELAY		60000 // 60 seconds autosleep
#define POWEROFF_DELAY	1000  // hold power 1s to power off

#define FB_SNAPSHOT_PATH "/tmp/fb_snapshot"
#define LOCKSCREEN_PATH "/mnt/sdcard/.system/rg35xxplus/bin/lockscreen.elf"
#define LOCKSCREEN_LOG "/mnt/sdcard/.userdata/rg35xxplus/logs/lockscreen.txt"
#define LOCKSCREEN_READY "/tmp/lockscreen_ready"

static int input_fd = 0;    // event1: volume/brightness/menu keys
static int power_fd = 0;    // event0: power button
static struct input_event ev;

static pthread_t hdmi_pt;
#define HDMI_STATE_PATH "/sys/class/extcon/hdmi/cable.0/state"

static void* watchHDMI(void *arg) {
	int has_hdmi,had_hdmi;

	has_hdmi = had_hdmi = pwrGetInt(HDMI_STATE_PATH);
	SetHDMI(has_hdmi);

	while(1) {
		sleep(1);

		has_hdmi = pwrGetInt(HDMI_STATE_PATH);
		if (had_hdmi!=has_hdmi) {
			had_hdmi = has_hdmi;
			SetHDMI(has_hdmi);
		}
	}

	return 0;
}

///////////////////////////////
// Lock/unlock orchestration

static pid_t find_pid(const char* name) {
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "pidof %s", name);
	FILE* f = popen(cmd, "r");
	pid_t pid = 0;
	if (f) { fscanf(f, "%d", &pid); pclose(f); }
	return pid;
}

static void snapshotFB(void) {
	system("cp /dev/fb0 " FB_SNAPSHOT_PATH);
}

static void restoreFB(void) {
	system("cp " FB_SNAPSHOT_PATH " /dev/fb0");
}

static pid_t launchLockscreen(void) {
	// Remove stale ready sentinel
	unlink(LOCKSCREEN_READY);

	pid_t pid = fork();
	if (pid == 0) {
		// Redirect stdout+stderr to log file
		int logfd = open(LOCKSCREEN_LOG, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (logfd >= 0) {
			dup2(logfd, STDOUT_FILENO);
			dup2(logfd, STDERR_FILENO);
			close(logfd);
		}
		execl(LOCKSCREEN_PATH, "lockscreen.elf", NULL);
		_exit(1); // exec failed
	}
	return pid;
}

// Wait for lockscreen to signal it has rendered its first frame (max 2s)
static void waitLockscreenReady(void) {
	for (int i = 0; i < 40; i++) { // 40 * 50ms = 2s max
		if (access(LOCKSCREEN_READY, F_OK) == 0) return;
		usleep(50000);
	}
}

#define AUTO_POWEROFF_MS 600000 // 10 minutes

// Drain all pending evdev events from keymon's input fds
static void drainEvdev(void) {
	struct input_event drain_ev;
	int fds[] = {power_fd, input_fd};
	for (int i = 0; i < 2; i++) {
		if (fds[i] < 0) continue;
		while (read(fds[i], &drain_ev, sizeof(drain_ev)) == sizeof(drain_ev)) {}
	}
}

// Wait for power button to wake from sleep.
// Auto-powers off after 10 minutes if not charging.
static void waitForWake(void) {
	struct pollfd pfd[2];
	int nfds = 0;
	struct input_event wake_ev;
	struct timeval tod;

	if (power_fd >= 0) { pfd[nfds].fd = power_fd; pfd[nfds].events = POLLIN; nfds++; }
	if (input_fd >= 0) { pfd[nfds].fd = input_fd; pfd[nfds].events = POLLIN; nfds++; }

	gettimeofday(&tod, NULL);
	uint32_t sleep_start = tod.tv_sec * 1000 + tod.tv_usec / 1000;

	while (1) {
		int timeout_ms = 60000; // check every 60s for auto-poweroff
		int ret = poll(pfd, nfds, timeout_ms);

		if (ret > 0) {
			for (int i = 0; i < nfds; i++) {
				if (!(pfd[i].revents & POLLIN)) continue;
				while (read(pfd[i].fd, &wake_ev, sizeof(wake_ev)) == sizeof(wake_ev)) {
					if (wake_ev.type == EV_KEY && wake_ev.code == CODE_POWER) return;
				}
			}
		}

		// Auto-poweroff check
		gettimeofday(&tod, NULL);
		uint32_t now = tod.tv_sec * 1000 + tod.tv_usec / 1000;
		if (now - sleep_start >= AUTO_POWEROFF_MS) {
			if (pwrIsCharging()) {
				sleep_start += 60000; // check again in a minute
			} else {
				// Poweroff: remove exec sentinel so launch.sh exits to shutdown
				system("rm -f /tmp/minui_exec && sync");
				exit(0);
			}
		}
	}
}

static void doLock(void) {
	// 1. Find foreground app PID
	pid_t app_pid = find_pid("minui.elf");
	if (!app_pid) app_pid = find_pid("minarch.elf");

	// 2. Snapshot framebuffer
	snapshotFB();

	// 3. SIGSTOP foreground app
	if (app_pid) kill(app_pid, SIGSTOP);

	// 4. Disable backlight, mute, sync (matches original PWR_enterSleep)
	SetRawVolume(0);
	pwrEnableBacklight(0);
	sync();

	while (1) {
		// 5. Launch lockscreen fresh each cycle (GPU context doesn't survive
		//    long FB_BLANK_POWERDOWN — relaunch ensures a clean SDL/EGL state)
		int status = 0;
		pid_t lock_pid = launchLockscreen();

		// 6. Wait until lockscreen signals its first frame is rendered
		waitLockscreenReady();

		// 7. Drain stale events (e.g. the power press that triggered sleep)
		drainEvdev();

		// 8. Wait for power button press
		waitForWake();

		// 9. Re-enable backlight (lockscreen frame is already on fb — instant)
		pwrEnableBacklight(1);
		SetVolume(GetVolume());

		// 10. Remove stale sentinel before signaling
		unlink(LOCKSCREEN_READY);

		// 11. Signal lockscreen to start accepting input
		if (lock_pid > 0) {
			kill(lock_pid, SIGUSR1);
		}

		// 12. Wait for lockscreen outcome:
		//   - Process exits → check exit status
		int unlocked = 0;
		while (1) {
			int ret = waitpid(lock_pid, &status, WNOHANG);
			if (ret > 0) {
				// exit(0) = unlocked, exit(2) = re-sleep
				if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
					unlocked = 1;
				break;
			}
			if (access(LOCKSCREEN_READY, F_OK) == 0) {
				// Lockscreen signaled re-sleep via sentinel
				break;
			}
			usleep(20000); // 20ms poll
		}

		// Kill lockscreen if still alive (re-sleep path)
		if (!unlocked && lock_pid > 0) {
			kill(lock_pid, SIGTERM);
			waitpid(lock_pid, NULL, 0);
		}

		if (unlocked) break;

		// Re-sleep: blank screen, mute, sync
		SetRawVolume(0);
		pwrEnableBacklight(0);
		sync();
	}

	// 13. Restore framebuffer
	restoreFB();

	// 14. SIGCONT foreground app
	if (app_pid) kill(app_pid, SIGCONT);
}

int main (int argc, char *argv[]) {
	InitSettings();
	pthread_create(&hdmi_pt, NULL, &watchHDMI, NULL);

	input_fd = open("/dev/input/event1", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	power_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);

	// Also read event1 for power — on rg35xxplus, power button (code 116)
	// comes through event1 alongside volume/menu keys

	uint32_t val;
	uint32_t menu_pressed = 0;

	uint32_t up_pressed = 0;
	uint32_t up_just_pressed = 0;
	uint32_t up_repeat_at = 0;

	uint32_t down_pressed = 0;
	uint32_t down_just_pressed = 0;
	uint32_t down_repeat_at = 0;

	uint32_t power_pressed = 0;
	uint32_t power_pressed_at = 0;

	uint8_t ignore;
	uint32_t then;
	uint32_t now;
	uint32_t last_input_at;
	struct timeval tod;

	gettimeofday(&tod, NULL);
	then = tod.tv_sec * 1000 + tod.tv_usec / 1000; // essential SDL_GetTicks()
	last_input_at = then;
	ignore = 0;

	while (1) {
		gettimeofday(&tod, NULL);
		now = tod.tv_sec * 1000 + tod.tv_usec / 1000;
		// TODO: check if if necessary
		if (now-then>1000) ignore = 1; // ignore input that arrived during sleep

		// Read event1 (volume/brightness/menu AND power button)
		while(read(input_fd, &ev, sizeof(ev))==sizeof(ev)) {
			if (ignore) continue;
			val = ev.value;

			if (( ev.type != EV_KEY ) || ( val > REPEAT )) continue;
			last_input_at = now;
			printf("event1 code: %i (%i)\n", ev.code, val); fflush(stdout);
			switch (ev.code) {
				case CODE_MENU:
					menu_pressed = val;
				break;
				break;
				case CODE_PLUS:
					up_pressed = up_just_pressed = val;
					if (val) up_repeat_at = now + 300;
				break;
				case CODE_MINUS:
					down_pressed = down_just_pressed = val;
					if (val) down_repeat_at = now + 300;
				break;
				case CODE_POWER:
					if (val == PRESSED) {
						power_pressed = 1;
						power_pressed_at = now;
					} else if (val == RELEASED) {
						if (power_pressed && now - power_pressed_at < POWEROFF_DELAY) {
							// Short press = sleep/lock
							power_pressed = 0;
							power_pressed_at = 0;
							doLock();
							// After return from lock, reset timing
							gettimeofday(&tod, NULL);
							now = tod.tv_sec * 1000 + tod.tv_usec / 1000;
							last_input_at = now;
							then = now;
							ignore = 1;
						}
						power_pressed = 0;
					}
				break;
				default:
				break;
			}
		}

		// Read event0 (dpad/face buttons — just for activity tracking + wake)
		while(read(power_fd, &ev, sizeof(ev))==sizeof(ev)) {
			if (ignore) continue;
			if (ev.type == EV_KEY && ev.value <= REPEAT) {
				last_input_at = now;
				printf("event0 code: %i (%i)\n", ev.code, ev.value); fflush(stdout);
				// Also check for power button on event0
				if (ev.code == CODE_POWER) {
					if (ev.value == PRESSED) {
						power_pressed = 1;
						power_pressed_at = now;
					} else if (ev.value == RELEASED) {
						if (power_pressed && now - power_pressed_at < POWEROFF_DELAY) {
							power_pressed = 0;
							power_pressed_at = 0;
							doLock();
							gettimeofday(&tod, NULL);
							now = tod.tv_sec * 1000 + tod.tv_usec / 1000;
							last_input_at = now;
							then = now;
							ignore = 1;
						}
						power_pressed = 0;
					}
				}
			}
		}

		// Long press power = power off (handled by the app's own PWR_update)
		// We don't do poweroff from keymon — the app owns that flow.

		if (ignore) {
			menu_pressed = 0;
			up_pressed = up_just_pressed = 0;
			down_pressed = down_just_pressed = 0;
			up_repeat_at = 0;
			down_repeat_at = 0;
			power_pressed = 0;
			power_pressed_at = 0;
		}

		if (up_just_pressed || (up_pressed && now>=up_repeat_at)) {
			if (menu_pressed) {
				// printf("brightness up\n"); fflush(stdout);
				val = GetBrightness();
				if (val<BRIGHTNESS_MAX) SetBrightness(++val);
			}
			else {
				// printf("volume up\n"); fflush(stdout);
				val = GetVolume();
				if (val<VOLUME_MAX) SetVolume(++val);
			}

			if (up_just_pressed) up_just_pressed = 0;
			else up_repeat_at += 100;
		}

		if (down_just_pressed || (down_pressed && now>=down_repeat_at)) {
			if (menu_pressed) {
				// printf("brightness down\n"); fflush(stdout);
				val = GetBrightness();
				if (val>BRIGHTNESS_MIN) SetBrightness(--val);
			}
			else {
				// printf("volume down\n"); fflush(stdout);
				val = GetVolume();
				if (val>VOLUME_MIN) SetVolume(--val);
			}

			if (down_just_pressed) down_just_pressed = 0;
			else down_repeat_at += 100;
		}

		// Autosleep: 60s of no input → lock
		if (!GetHDMI() && now - last_input_at >= SLEEP_DELAY) {
			doLock();
			gettimeofday(&tod, NULL);
			now = tod.tv_sec * 1000 + tod.tv_usec / 1000;
			last_input_at = now;
			then = now;
			ignore = 1;
			continue; // restart loop to flush stale input
		}

		then = now;
		ignore = 0;

		usleep(16666); // 60fps
	}
}
