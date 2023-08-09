#include "Configuration.h"
#include <Arduino.h>
#include <Wire.h>
#if FIS_SENSOR == FIS_MLX90621
  #include "MLX90621.h"
#elif FIS_SENSOR == FIS_MLX90640
  #include "MLX90640.h"
#elif FIS_SENSOR == FIS_AMG8833
  #include "Melopero_AMG8833.h"
#elif FIS_SENSOR == FIS_MLX90614
  #include <Adafruit_MLX90614.h>
#endif

#define ABS_ZERO -2732
#define MIN_TMP_STDDEV ((25 + TEMPOFFSET) * 10 * TEMPSCALING) // minimum standard deviation we use for outlier detection (25 degrees Celsius)
#define TMP_TRIGGER_DELTA_AMBIENT_TIRE (7 * 10 * TEMPSCALING) // delta temperature threshold for detecting a tire edge
#define TMP_AVG_DELTA_AMBIENT_TIRE (5 * 10 * TEMPSCALING) // tire needs to be significantly warmer than ambient temp

typedef struct {
  float  avgFrameTemp = 0; // after normalizing to one row; full width = avg(FIS_X)
  float  stdDevFrameTemp = 0; // after normalizing to one row; full width = avg(FIS_X)
  float  avgMinFrameTemp = 0; // after normalizing to one row; full width = avg(FIS_X)
  float  avgMaxFrameTemp = 0; // after normalizing to one row; full width = avg(FIS_X)

  float  avgTireTemp = 0; // 0 if no valid autozoom frame
  float  avgOuterTireTemp = 0; // 1/3 of outermost tire pixels; 0 if no valid autozoom frame 
  float  avgMiddleTireTemp = 0; // 1/3 of middle tire pixels; 0 if no valid autozoom frame
  float  avgInnerTireTemp = 0; // 1/3 of innermost tire pixels; 0 if no valid autozoom frame
  float  avgOuterAmbientTemp = 0; // 0 if no valid autozoom frame
  float  avgInnerAmbientTemp = 0; // 0 if no valid autozoom frame
} avgTemps_t; // all temps in degrees Celsius x 10


class TempSensor {
private:
#if FIS_SENSOR == FIS_MLX90621
  MLX90621 FISDevice;
#elif FIS_SENSOR == FIS_MLX90640
  MLX90640 FISDevice;
#elif FIS_SENSOR == FIS_AMG8833
  Melopero_AMG8833 FISDevice;
#elif FIS_SENSOR == FIS_MLX90614
  Adafruit_MLX90614 FISDevice;
#endif
  
  TwoWire *thisWire;

  int16_t getPixelTemperature(uint8_t x, uint8_t y);
  int16_t calculateColumnTemperature(int16_t column_content[], uint8_t size);
  void interpolate(uint8_t startColumn, uint8_t endColumn, int16_t result[]);
  void calculateSlope(int16_t result[]);
  void getMinMaxSlopePosition();
  boolean checkAutozoomValidityAndSetAvgTemps();
  int16_t getMaximum(int16_t arr[], int size);
  int16_t getMinimum(int16_t arr[], int size);
  float getAverage(int16_t arr[], int size);
  float getGeometricMean(int16_t arr[], int size);
  float getStdDev(int16_t arr[], int size);
  float cumulativeProbability(float val, float avg, float stdDev);
  uint16_t removeOutliersChauvenet(int16_t *arr, int size);
  
public:
  int16_t measurement[FIS_X];
  int16_t measurement_slope[FIS_X-1];
  int16_t measurement_16[16];
  boolean validAutozoomFrame = false;
  uint8_t outerTireEdgePositionThisFrameViaSlopeMax; // i.e. the index of the first pixel _on_ the tire as detected for this measurement; corresponds to Max value in the Slope
  uint8_t innerTireEdgePositionThisFrameViaSlopeMin; // i.e. the index of the last pixel _on_ the tire as detected for this measurement; corresponds to Min value in the Slope
  float outerTireEdgePositionSmoothed; // outer = left = array index 0
  float innerTireEdgePositionSmoothed; // inner = right = array index FIS_X

  avgTemps_t avgsThisFrame;
  uint16_t totalOutliersThisFrame = 0;

  // Let's keep track of some history of this session...
  double totalFrameCount = 0.0;
  float runningAvgOutlierRate = 0.0;
  float runningAvgZoomedFramesRate = 0.0;
  float movingAvgFrameTmp = (40.0 + TEMPOFFSET) * 10 * TEMPSCALING; // init value = 40 degrees Celsius
  float movingAvgStdDevFrameTmp = MIN_TMP_STDDEV;
  float movingAvgRowDeltaTmp = 0.0; // delta between all lowest row values vs. all highest row values of the frames
  float maxRowDeltaTmp = 0.0; // maximum of the moving average to detect shaded rows through increasing deltas with increasingly warm tire temps vs. constantly cold bodywork
  
	boolean initialise(int refrate, TwoWire *I2Cpipe = &Wire);
	void measure();
};
