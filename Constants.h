// FIS identifiers
#define FIS_MLX90621 0
#define FIS_MLX90640 1
#define FIS_DUMMY    2
#define FIS_AMG8833  3
#define FIS_MLX90614 4   // Make sure the Adafruit MLX90614 library is installed when using this

// Distance sensor identifiers
#define DIST_VL53L0X 0
#define DIST_NONE    1

// Display device
#define DISP_128X32 0
#define DISP_NONE   1

// Boards
#define BOARD_NRF52_FEATHER 0
#define BOARD_ESP32_FEATHER 1
#define BOARD_ESP32_LOLIND32 2

// Column aggregation algo identifiers
#define COLUMN_AGGREGATE_MAX 0
#define COLUMN_AGGREGATE_AVG 1
#define COLUMN_AGGREGATE_AVG_MINUS_OUTLIERS 2

// Voltage Measurement related items

#ifdef _DEBUG
  #define debug(fmt, ...) { Serial.printf(fmt,## __VA_ARGS__); }
#else
  #define debug(fmt, ...) {} // Silence is golden
#endif
