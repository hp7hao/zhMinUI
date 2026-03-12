/*
 * minwm — DirectFB2 compositor & app manager for MinUI (rg35xxplus)
 *
 * The central always-running master process.  Owns the primary display
 * layer (DLSCL_ADMINISTRATIVE), manages the lockscreen overlay, and
 * drives the minui/minarch app lifecycle loop:
 *
 *   1. Spawn minui.elf
 *   2. Run event loop (lockscreen + idle + signals) while child runs
 *   3. When minui exits, check /tmp/next for a game to launch
 *   4. If game requested: spawn it, same event loop, then cleanup
 *   5. Repeat until /tmp/minui_exec sentinel is removed (→ shutdown)
 *
 * Lock/unlock controlled by:
 *   - SIGUSR1 from keymon → lock
 *   - SIGUSR2 from keymon → unlock
 *   - 3 button presses    → unlock
 *   - Idle timeout         → re-lock
 */

#include <direct/clock.h>
#include <direct/util.h>
#include <directfb.h>
#include <directfb_util.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <execinfo.h>

/* macro for a safe call to DirectFB functions */
#define DFBCHECK(x)                                                   \
     do {                                                             \
          DFBResult result = x;                                       \
          if (result != DFB_OK) {                                     \
               fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
               DirectFBErrorFatal( #x, result );                      \
          }                                                           \
     } while (0)

/* --- Configuration --- */
#define UNLOCK_PRESSES     3
#define IDLE_TIMEOUT_MS    30000   /* 30s inactivity → re-lock */
#define LOCK_BG_ALPHA      0xC0
#define EVENT_TIMEOUT_MS   50      /* 50ms poll */

/* --- Signal handlers --- */
static volatile sig_atomic_t want_lock   = 0;
static volatile sig_atomic_t want_unlock = 0;

static void sig_lock(int s)   { (void)s; want_lock = 1; }
static void sig_unlock(int s) { (void)s; want_unlock = 1; }

static void sig_crash(int sig)
{
     void *bt[20];
     int n = backtrace(bt, 20);
     fprintf(stderr, "minwm: caught signal %d, backtrace:\n", sig);
     backtrace_symbols_fd(bt, n, STDERR_FILENO);
     _exit(128 + sig);
}

static void install_signal_handlers(void)
{
     struct sigaction sa;
     memset(&sa, 0, sizeof(sa));
     sa.sa_handler = sig_lock;
     sigaction(SIGUSR1, &sa, NULL);
     sa.sa_handler = sig_unlock;
     sigaction(SIGUSR2, &sa, NULL);
     /* Crash diagnostics — catch SIGSEGV/SIGBUS/SIGABRT for backtraces */
     sa.sa_handler = sig_crash;
     sa.sa_flags = SA_RESETHAND;  /* reset to default after first signal */
     sigaction(SIGSEGV, &sa, NULL);
     sigaction(SIGBUS, &sa, NULL);
     sigaction(SIGABRT, &sa, NULL);
     /* Ignore SIGTERM, SIGINT, SIGHUP — minwm exits only when the
        /tmp/minui_exec sentinel is removed (by PLAT_powerOff).
        SIGHUP must be re-ignored here because DirectFBCreate()
        installs its own signal handlers that override the earlier
        signal(SIGHUP, SIG_IGN) call. */
     sa.sa_handler = SIG_IGN;
     sa.sa_flags = 0;
     sigaction(SIGTERM, &sa, NULL);
     sigaction(SIGINT, &sa, NULL);
     sigaction(SIGHUP, &sa, NULL);
}

/* --- DirectFB globals --- */
static IDirectFB             *dfb          = NULL;
static IDirectFBDisplayLayer *layer        = NULL;
static IDirectFBWindow       *lock_window  = NULL;
static IDirectFBSurface      *lock_surface = NULL;
static IDirectFBEventBuffer  *evbuf        = NULL;
static IDirectFBFont         *font         = NULL;

static void dfb_shutdown(void)
{
     if (font)         font->Release(font);
     if (evbuf)        evbuf->Release(evbuf);
     if (lock_surface)  lock_surface->Release(lock_surface);
     if (lock_window)   lock_window->Release(lock_window);
     if (layer) {
          layer->SetCooperativeLevel(layer, DLSCL_SHARED);
          layer->Release(layer);
     }
     if (dfb) dfb->Release(dfb);
}

/* --- App lifecycle paths --- */
#define EXEC_PATH "/tmp/minui_exec"
#define NEXT_PATH "/tmp/next"

static int file_exists(const char *path)
{
     return access(path, F_OK) == 0;
}

static pid_t spawn_child(const char *cmd)
{
     pid_t pid = fork();
     if (pid == 0) {
          execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
          _exit(127);
     }
     if (pid < 0)
          perror("minwm: fork");
     else
          fprintf(stderr, "minwm: spawned child %d: %s\n", (int)pid, cmd);
     return pid;
}

/* Spawn a binary directly (no shell). Searches PATH. Redirects stdout/stderr to logfile. */
static pid_t spawn_bin(const char *name, const char *logfile)
{
     pid_t pid = fork();
     if (pid == 0) {
          if (logfile) {
               int fd = open(logfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
               if (fd >= 0) {
                    dup2(fd, STDOUT_FILENO);
                    dup2(fd, STDERR_FILENO);
                    close(fd);
               }
          }
          execlp(name, name, (char *)NULL);
          perror("minwm: execlp");
          _exit(127);
     }
     if (pid < 0)
          perror("minwm: fork");
     else
          fprintf(stderr, "minwm: spawned child %d: %s\n", (int)pid, name);
     return pid;
}

static void write_datetime(const char *path)
{
     if (!path)
          return;
     FILE *f = fopen(path, "w");
     if (f) {
          time_t now = time(NULL);
          struct tm *tm = localtime(&now);
          fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d\n",
                  tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                  tm->tm_hour, tm->tm_min, tm->tm_sec);
          fclose(f);
     }
     sync();
}

/* Draw the lockscreen content onto the lock surface */
static void draw_lockscreen(int width, int height)
{
     if (!lock_surface || !font)
          return;

     int fontheight;
     font->GetHeight(font, &fontheight);

     /* dark semi-transparent backdrop */
     lock_surface->SetColor(lock_surface, 0x00, 0x00, 0x00, LOCK_BG_ALPHA);
     lock_surface->FillRectangle(lock_surface, 0, 0, width, height);

     /* lock icon: simple padlock using rectangles */
     int cx = width / 2;
     int cy = height / 2 - fontheight;
     int box_w = 40, box_h = 30;
     int arc_w = 24, arc_h = 20, arc_t = 4;

     /* shackle (arch) */
     lock_surface->SetColor(lock_surface, 0xFF, 0xFF, 0xFF, 0xFF);
     lock_surface->FillRectangle(lock_surface, cx - arc_w / 2, cy - arc_h - box_h / 2, arc_t, arc_h);
     lock_surface->FillRectangle(lock_surface, cx + arc_w / 2 - arc_t, cy - arc_h - box_h / 2, arc_t, arc_h);
     lock_surface->FillRectangle(lock_surface, cx - arc_w / 2, cy - arc_h - box_h / 2, arc_w, arc_t);

     /* lock body */
     lock_surface->SetColor(lock_surface, 0xDD, 0xDD, 0xDD, 0xFF);
     lock_surface->FillRectangle(lock_surface, cx - box_w / 2, cy - box_h / 2, box_w, box_h);

     /* text */
     lock_surface->SetColor(lock_surface, 0xFF, 0xFF, 0xFF, 0xFF);
     lock_surface->DrawString(lock_surface,
                              "Screen Locked",
                              -1, cx, cy + box_h / 2 + fontheight + 4,
                              DSTF_CENTER);

     lock_surface->SetColor(lock_surface, 0xAA, 0xAA, 0xAA, 0xFF);
     lock_surface->DrawString(lock_surface,
                              "Press any button 3 times to unlock",
                              -1, cx, cy + box_h / 2 + fontheight * 2 + 8,
                              DSTF_CENTER);

     lock_surface->Flip(lock_surface, NULL, DSFLIP_NONE);
}

static void show_lockscreen(void)
{
     if (lock_window) {
          /* Raise and focus BEFORE making visible — SetOpacity(0→0xFF)
             triggers a full-screen WM repaint.  If we raise afterwards,
             the restack triggers a second repaint.  Doing it in this
             order: raise (no repaint, window is invisible) → opacity
             (single repaint with correct z-order) → focus. */
          lock_window->RaiseToTop(lock_window);
          lock_window->SetOpacity(lock_window, 0xFF);
          lock_window->RequestFocus(lock_window);
     }
}

static void hide_lockscreen(void)
{
     if (lock_window)
          lock_window->SetOpacity(lock_window, 0x00);
}

/*
 * event_loop_tick — one iteration of the event loop.
 * Checks child exit, processes lock/unlock signals, polls DFB input,
 * and handles idle timeout.  Returns 1 if child exited, 0 otherwise.
 */
static int event_loop_tick(pid_t *child_pid, int *lock_visible,
                           unsigned int *unlock_count,
                           unsigned int *last_input_ms)
{
     /* Check if child exited */
     int status;
     pid_t w = waitpid(*child_pid, &status, WNOHANG);
     if (w > 0) {
          if (WIFEXITED(status))
               fprintf(stderr, "minwm: child %d exited normally (status %d)\n",
                       (int)w, WEXITSTATUS(status));
          else if (WIFSIGNALED(status))
               fprintf(stderr, "minwm: child %d killed by signal %d\n",
                       (int)w, WTERMSIG(status));
          else
               fprintf(stderr, "minwm: child %d exited (raw status 0x%x)\n",
                       (int)w, status);
          *child_pid = 0;
          return 1;
     }

     /* External signal: lock */
     if (want_lock) {
          want_lock = 0;
          if (!*lock_visible) {
               *lock_visible = 1;
               *unlock_count = 0;
               show_lockscreen();
               fprintf(stderr, "minwm: locked (signal)\n");
          }
     }

     /* External signal: unlock */
     if (want_unlock) {
          want_unlock = 0;
          if (*lock_visible) {
               *lock_visible = 0;
               *unlock_count = 0;
               hide_lockscreen();
               *last_input_ms = (unsigned int)direct_clock_get_millis();
               fprintf(stderr, "minwm: unlocked (signal)\n");
          }
     }

     /* Poll the global input buffer (non-blocking to avoid Fusion lock) */
     DFBInputEvent evt;
     if (evbuf->HasEvent(evbuf) != DFB_OK)
          usleep(EVENT_TIMEOUT_MS * 1000);

     while (evbuf->GetEvent(evbuf, DFB_EVENT(&evt)) == DFB_OK) {
          if (*lock_visible) {
               if (evt.type == DIET_KEYPRESS) {
                    (*unlock_count)++;
                    fprintf(stderr, "minwm: unlock press %u/%u\n",
                            *unlock_count, (unsigned)UNLOCK_PRESSES);
                    if (*unlock_count >= UNLOCK_PRESSES) {
                         *lock_visible = 0;
                         *unlock_count = 0;
                         hide_lockscreen();
                         *last_input_ms = (unsigned int)direct_clock_get_millis();
                         fprintf(stderr, "minwm: unlocked (button)\n");
                    }
               }
          } else {
               *last_input_ms = (unsigned int)direct_clock_get_millis();
          }
     }

     /* Idle timeout → re-lock */
     if (!*lock_visible) {
          unsigned int now_ms = (unsigned int)direct_clock_get_millis();
          if (now_ms - *last_input_ms >= IDLE_TIMEOUT_MS) {
               *lock_visible = 1;
               *unlock_count = 0;
               show_lockscreen();
               fprintf(stderr, "minwm: locked (idle timeout)\n");
          }
     }

     return 0;
}

int main(int argc, char *argv[])
{
     DFBDisplayLayerConfig  config;
     DFBWindowDescription   wdsc;
     DFBFontDescription     fdsc;

     /* Ignore SIGHUP before DirectFB init — minwm runs as a background
        process and receives SIGHUP when the parent shell exits.
        DirectFB2 installs its own signal handlers that would trigger
        emergency shutdown on SIGHUP. */
     signal(SIGHUP, SIG_IGN);

     /* Initialize DirectFB */
     DFBCHECK(DirectFBInit(&argc, &argv));
     DFBCHECK(DirectFBCreate(&dfb));

     atexit(dfb_shutdown);
     install_signal_handlers();

     /* Get primary display layer and take administrative control */
     DFBCHECK(dfb->GetDisplayLayer(dfb, DLID_PRIMARY, &layer));
     DFBCHECK(layer->SetCooperativeLevel(layer, DLSCL_ADMINISTRATIVE));

     DFBCHECK(layer->GetConfiguration(layer, &config));
     int screen_w = config.width;
     int screen_h = config.height;

     /* Enable double-buffered compositing on the layer.
        With DLBM_FRONTONLY (the fbdev default), the WM composites directly
        to the live framebuffer — partial compositing (background → app →
        lockscreen) is visible as flashing/tearing.
        DLBM_BACKVIDEO gives front+back buffers; the WM composites to
        back, then atomically pan-flips to front.  Falls back to
        BACKSYSTEM (CPU-side back buffer) if the kernel fb driver lacks
        ypanstep support, and to FRONTONLY if both fail. */
     {
          DFBDisplayLayerConfig lc;
          lc.flags      = DLCONF_BUFFERMODE;
          lc.buffermode = DLBM_BACKVIDEO;
          DFBResult bret = layer->SetConfiguration(layer, &lc);
          if (bret != DFB_OK) {
               fprintf(stderr, "minwm: DLBM_BACKVIDEO failed (%s), trying BACKSYSTEM\n",
                       DirectFBErrorString(bret));
               lc.buffermode = DLBM_BACKSYSTEM;
               bret = layer->SetConfiguration(layer, &lc);
               if (bret != DFB_OK)
                    fprintf(stderr, "minwm: DLBM_BACKSYSTEM also failed (%s), staying FRONTONLY\n",
                            DirectFBErrorString(bret));
               else
                    fprintf(stderr, "minwm: layer buffermode set to BACKSYSTEM\n");
          }
          else {
               fprintf(stderr, "minwm: layer buffermode set to BACKVIDEO\n");
          }
     }

     /* Set black background, disable cursor */
     layer->SetBackgroundColor(layer, 0, 0, 0, 0xFF);
     layer->SetBackgroundMode(layer, DLBM_COLOR);
     layer->EnableCursor(layer, 0);

     fprintf(stderr, "minwm: display %dx%d, compositor starting\n",
             screen_w, screen_h);

     /* Load font — try system TTF first, fall back to DFB default */
     const char *font_path = getenv("MINWM_FONT");
     if (!font_path)
          font_path = "/mnt/sdcard/.system/res/fonts/BoutiqueBitmap7x7_1.7.ttf";

     fdsc.flags  = DFDESC_HEIGHT;
     fdsc.height = 16;

     DFBResult fret = dfb->CreateFont(dfb, font_path, &fdsc, &font);
     if (fret != DFB_OK) {
          fprintf(stderr, "minwm: could not load font '%s' (%s), trying fallback\n",
                  font_path, DirectFBErrorString(fret));
          /* Try a generic path */
          fret = dfb->CreateFont(dfb, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                 &fdsc, &font);
          if (fret != DFB_OK) {
               fprintf(stderr, "minwm: no font available, lockscreen text disabled\n");
               font = NULL;
          }
     }

     /* Create lockscreen window: full-screen, UPPER stacking */
     memset(&wdsc, 0, sizeof(wdsc));
     wdsc.flags        = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT |
                          DWDESC_CAPS | DWDESC_SURFACE_CAPS | DWDESC_STACKING;
     wdsc.posx          = 0;
     wdsc.posy          = 0;
     wdsc.width         = screen_w;
     wdsc.height        = screen_h;
     wdsc.caps          = DWCAPS_ALPHACHANNEL;
     wdsc.surface_caps  = DSCAPS_PREMULTIPLIED;
     wdsc.stacking      = DWSC_UPPER;

     DFBCHECK(layer->CreateWindow(layer, &wdsc, &lock_window));
     DFBCHECK(lock_window->GetSurface(lock_window, &lock_surface));

     if (font)
          lock_surface->SetFont(lock_surface, font);

     /* Pre-render lockscreen content, start hidden */
     draw_lockscreen(screen_w, screen_h);
     hide_lockscreen();

     /* Global input event buffer — receives ALL input events from all
        DirectFB input drivers, regardless of window focus. Used for:
        - Counting button presses to unlock (when locked)
        - Tracking idle timeout to re-lock (when unlocked) */
     DFBCHECK(dfb->CreateInputEventBuffer(dfb, DICAPS_ALL, DFB_TRUE, &evbuf));

     fprintf(stderr, "minwm: compositor ready, lockscreen hidden\n");

     /* --- App lifecycle state --- */
     int          lock_visible = 0;
     unsigned int unlock_count = 0;
     unsigned int last_input_ms = (unsigned int)direct_clock_get_millis();

     /* Read environment for app lifecycle */
     const char *logs_path     = getenv("LOGS_PATH");
     const char *datetime_path = getenv("DATETIME_PATH");
     if (!logs_path)     logs_path     = "/tmp";
     if (!datetime_path) datetime_path = NULL;

     /* Create sentinel — outer loop runs while this exists */
     {
          FILE *f = fopen(EXEC_PATH, "w");
          if (f) fclose(f);
          sync();
     }

     fprintf(stderr, "minwm: entering app lifecycle loop\n");

     /* === OUTER LOOP: app lifecycle === */
     int consecutive_crashes = 0;
     while (file_exists(EXEC_PATH)) {

          /* --- Spawn hellodfb (temporary test) --- */
          char minui_log[256];
          snprintf(minui_log, sizeof(minui_log), "%s/minui.txt", logs_path);
          pid_t child_pid = spawn_bin("hellodfb.elf", minui_log);
          if (child_pid < 0) {
               fprintf(stderr, "minwm: failed to spawn hellodfb, retrying in 1s\n");
               sleep(1);
               continue;
          }

          unsigned int spawn_time = (unsigned int)direct_clock_get_millis();

          /* --- INNER LOOP: event loop while minui runs --- */
          while (child_pid > 0) {
               if (event_loop_tick(&child_pid, &lock_visible,
                                   &unlock_count, &last_input_ms))
                    break;
          }

          /* minui exited — record datetime */
          write_datetime(datetime_path);

          /* Crash backoff: if child died within 2s, it likely crashed.
             Delay before respawn to avoid tight loop that starves
             the event loop (lockscreen becomes unresponsive). */
          unsigned int run_ms = (unsigned int)direct_clock_get_millis() - spawn_time;
          if (run_ms < 2000) {
               consecutive_crashes++;
               unsigned int delay = consecutive_crashes < 5 ? 1 : 3;
               fprintf(stderr, "minwm: child crashed after %ums (crash #%d), "
                       "waiting %us before respawn\n",
                       run_ms, consecutive_crashes, delay);
               /* Process events during the delay so lockscreen stays responsive */
               unsigned int wait_until = (unsigned int)direct_clock_get_millis() + delay * 1000;
               while ((unsigned int)direct_clock_get_millis() < wait_until) {
                    event_loop_tick(&child_pid, &lock_visible,
                                   &unlock_count, &last_input_ms);
               }
          } else {
               consecutive_crashes = 0;
          }

          /* --- If /tmp/next exists, launch the game --- */
          if (file_exists(NEXT_PATH)) {
               char game_cmd[512];
               snprintf(game_cmd, sizeof(game_cmd),
                        ". /tmp/next > %s/next.txt 2>&1",
                        logs_path);
               pid_t game_pid = spawn_child(game_cmd);
               if (game_pid > 0) {
                    /* --- INNER LOOP: event loop while game runs --- */
                    while (game_pid > 0) {
                         if (event_loop_tick(&game_pid, &lock_visible,
                                            &unlock_count, &last_input_ms))
                              break;
                    }
               }

               unlink(NEXT_PATH);
               write_datetime(datetime_path);
          }
     }

     /* /tmp/minui_exec was removed — trigger shutdown */
     fprintf(stderr, "minwm: sentinel removed, shutting down\n");
     system("shutdown");
     return 0;
}
