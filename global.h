
static const char *TAG = "example";

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD/VGA spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define LCD_PIXEL_CLOCK_HZ     25000000
#define PIN_NUM_BK_LIGHT       -1
#define PIN_NUM_HSYNC          20
#define PIN_NUM_VSYNC          19
#define PIN_NUM_DE             -1
#define PIN_NUM_DISP_EN        -1
#define PIN_NUM_PCLK           999 // >100 for vga or real pin CLK for lcd
#define PIN_NUM_DATA11         (4)  // R0
#define PIN_NUM_DATA12         (5)  // R1
#define PIN_NUM_DATA13         (6)  // R2
#define PIN_NUM_DATA14         (7)  // R3
#define PIN_NUM_DATA15         (15) // R4
#define PIN_NUM_DATA5          (16) // G0
#define PIN_NUM_DATA6          (17) // G1
#define PIN_NUM_DATA7          (18) // G2
#define PIN_NUM_DATA8          (8)  // G3
#define PIN_NUM_DATA9          (3)  // G4
#define PIN_NUM_DATA10         (9)  // G5
#define PIN_NUM_DATA0          (10) // B0
#define PIN_NUM_DATA1          (11) // B1
#define PIN_NUM_DATA2          (12) // B2
#define PIN_NUM_DATA3          (13) // B3
#define PIN_NUM_DATA4          (14) // B4


// The pixel number in horizontal, vertical and synchronizations vartiables
int lcd_h_res = 640;
int lcd_v_res = 480;
int hsync_front_porch = 16;
int hsync_pulse_width = 96;
int hsync_back_porch = 48;
int vsync_front_porch = 10;
int vsync_pulse_width = 2;
int vsync_back_porch = 33;
int bounceBufferLines = lcd_v_res/10;

#define LVGL_TICK_PERIOD_MS    10
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 10
#define LVGL_TASK_STACK_SIZE   (4 * 1024)
#define LVGL_TASK_PRIORITY     20
