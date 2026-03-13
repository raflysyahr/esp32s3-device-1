#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* =========================================================
   CONFIG (boleh diubah sebelum include jika perlu)
   ========================================================= */

#ifndef LCD_COLS
#define LCD_COLS 16
#endif

#ifndef LCD_ROWS
#define LCD_ROWS 2
#endif


/* =========================================================
   INITIALIZATION
   ========================================================= */

/**
 * @brief Initialize LCD display manager
 *
 * Creates queue, history buffer, and LCD task.
 * Call once from app_main().
 */
void lcd_manager_init(void);


/* =========================================================
   LOG SYSTEM (THREAD SAFE)
   ========================================================= */

/**
 * @brief Send text to LCD log system
 *
 * Safe to call from any FreeRTOS task.
 * Message will be queued and displayed sequentially.
 *
 * @param msg null-terminated string
 */
void lcd_log(const char *msg);


/* =========================================================
   SCROLL CONTROL (REMOTE COMMAND READY)
   ========================================================= */

/**
 * @brief Scroll display up (show older logs)
 */
void lcd_scroll_up(void);

/**
 * @brief Scroll display down (show newer logs)
 */
void lcd_scroll_down(void);

/**
 * @brief Return view to latest logs
 */
void lcd_scroll_latest(void);


/* =========================================================
   OPTIONAL COMMAND HELPER
   ========================================================= */

/**
 * @brief Parse simple command string
 *
 * Supported:
 *  "UP"
 *  "DOWN"
 *  "LATEST"
 */
void handle_command(const char *cmd);


#ifdef __cplusplus
}
#endif

#endif /* LCD_MANAGER_H */
