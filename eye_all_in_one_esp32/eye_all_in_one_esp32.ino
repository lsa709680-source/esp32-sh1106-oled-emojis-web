// -----------------------------------------------------------------------------
// ESP32 OLED Emotive Eyes - Single-file sketch (one tab)
// - SoftAP: connect phone to ESP32-EYES, open http://192.168.4.1/
// -----------------------------------------------------------------------------

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WebServer.h>

// --------------------------- User config ---------------------------
static const int OLED_SDA = 21;   // change if needed
static const int OLED_SCL = 22;   // change if needed
static const uint8_t OLED_ADDR_7BIT = 0x3C;
static const char* AP_SSID = "ESP32-EYES";
static const char* AP_PASS = "12345678";
static const uint16_t EYES_FRAME_MS = 33; // ~30 FPS

// OLED driver (hardware I2C)
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// =========================== Headers ===========================


// ===== AsyncTimer.h =====
#ifndef _ASYNCTIMER_h
#define _ASYNCTIMER_h

#include <Arduino.h>

typedef void(*AsyncTimerCallback)();

class AsyncTimer {
 public:
	AsyncTimer(unsigned long millisInterval);
	AsyncTimer(unsigned long millisInterval, AsyncTimerCallback OnFinish);

	void Start();
	void Reset();
	void Stop();
	bool Update();

	void SetIntervalMillis(unsigned long interval);
	
	unsigned long GetStartTime();
	unsigned long GetElapsedTime();
	unsigned long GetRemainingTime();

	bool IsActive() const;
	bool IsExpired() const;
	
	unsigned long Interval;
	
	AsyncTimerCallback OnFinish;

private:
	bool _isActive;
	bool _isExpired;
	unsigned long _startTime;
};
#endif

// ===== Animations.h =====
#ifndef _ANIMATIONS_h
#define _ANIMATIONS_h

#include <Arduino.h>

class IAnimation {
public:
	virtual float GetValue() = 0;
	virtual float GetValue(unsigned long overWriteMillis) = 0;
	virtual unsigned long GetElapsed() = 0;

private:
	virtual float Calculate(unsigned long elapsedMillis) = 0;
};

class AnimationBase : IAnimation {
  public:
	  AnimationBase(unsigned long interval) : Interval(interval), StarTime(millis()) {}

	  unsigned long Interval;
	  unsigned long StarTime;

	  virtual void Restart() {
		  StarTime = millis();
	  }
	  float GetValue() override final {
		  return GetValue(GetElapsed());
	  }
	  float GetValue(unsigned long elapsedMillis) override final {
		  return Calculate(elapsedMillis);
	  }
	  unsigned long GetElapsed() override {
		  return static_cast<unsigned long> (millis() - StarTime);
	  }

  protected:
	  float Calculate(unsigned long elapsedMillis) override { return 0.0; }
};

class DeltaAnimation : public AnimationBase {
  public:
	  unsigned long StarTime;
	  DeltaAnimation(unsigned long interval) : AnimationBase(interval) {};
	  float Calculate(unsigned long elapsedMillis) {
		  if (elapsedMillis < Interval)	{
			  return 0.0f;
		  }
		  else {
			  return 1.0f;
		  }
	  };
};


class StepAnimation : public AnimationBase {
  public:
	  unsigned long Interval;
	  unsigned long StarTime;
	  bool IsActive = true;
  	StepAnimation(unsigned long interval) : AnimationBase(interval) {};

	  float Calculate(unsigned long elapsedMillis) {
		  if (elapsedMillis < Interval)	{
			  return 0.0f;
		  }
		  return 1.0f;
	  };
};


class RampAnimation : public AnimationBase {
public:

	unsigned long StarTime;
	bool IsActive = true;

	RampAnimation(unsigned long interval) : AnimationBase(interval) {};

	float Calculate(unsigned long elapsedMillis) {
		if (elapsedMillis < Interval)	{
			return static_cast<float>(elapsedMillis) / Interval;
		}
		return 1.0f;
	};
};

class TriangleAnimation : public AnimationBase {
public:

	TriangleAnimation(unsigned long interval) : AnimationBase(interval) {
		_t0 = interval / 2;
		_t1 = interval - _t0;
	}
	TriangleAnimation(unsigned long t0, unsigned long t1) : AnimationBase(t0 + t1) {
		_t0 = t0;
		_t1 = t1;
	};
	float Calculate(unsigned long elapsedMillis) {
		if (elapsedMillis % Interval < _t0) {
			return static_cast<float>(elapsedMillis % Interval) / _t0;
		}
		return 1.0f - (static_cast<float>(elapsedMillis % Interval) - _t0) / _t1;
	};
	unsigned long _t0;
	unsigned long _t1;
};


class TrapeziumAnimation : public AnimationBase {
public:
	TrapeziumAnimation(unsigned long t) : AnimationBase(t) {
		_t0 = t / 3;
		_t1 = _t0;
		_t2 = t - _t0 - _t1;
	};
	TrapeziumAnimation(unsigned long t0, unsigned long t1, unsigned long t2) : AnimationBase(t0 + t1 + t2) {
		_t0 = t0;
		_t1 = t1;
		_t2 = t2;
	}; 
	float Calculate(unsigned long elapsedMillis) override {
		if (elapsedMillis > Interval) return 0.0;
		if (elapsedMillis < _t0) {
			return static_cast<float>(elapsedMillis) / _t0;
		}
		else if (elapsedMillis < _t0 + _t1) {
			return 1.0f;
		}
		else {
			return 1.0f - (static_cast<float>(elapsedMillis) - _t1 - _t0) / _t2;
		}
	};

	unsigned long _t0;
	unsigned long _t1;
	unsigned long _t2;
};


class TrapeziumPulseAnimation : public AnimationBase {
public:
	TrapeziumPulseAnimation(unsigned long t) : AnimationBase(t) {
		_t0 = 0;
		_t1 = t / 3;
		_t2 = t - _t0 - _t0;
		_t3 = _t1;
		_t4 = 0;
	};

	TrapeziumPulseAnimation(unsigned long t0, unsigned long t1, unsigned long t2) : AnimationBase(t0 + t1 + t2) {
		_t0 = 0;
		_t1 = t0;
		_t2 = t1;
		_t3 = t2;
		_t4 = 0;
	};

	TrapeziumPulseAnimation(unsigned long t0, unsigned long t1, unsigned long t2, unsigned long t3, unsigned long t4) : AnimationBase(t0 + t1 + t2 + t3 + t4) {
		_t0 = t0;
		_t1 = t1;
		_t2 = t2;
		_t3 = t3;
		_t4 = t4;
	};

	float Calculate(unsigned long elapsedMillis) override {
		unsigned long elapsed = elapsedMillis % Interval;

		if (elapsed < _t0) {
			return 0.0;
		}
		if (elapsed < _t0 + _t1) {
			return static_cast<float>(elapsed - _t0) / _t1;
		}
		else if (elapsed < _t0 + _t1 + _t2)	{
			return 1.0f;
		}
		else if (elapsed < _t0 + _t1 + _t2 + _t3)	{
			return 1.0f - (static_cast<float>(elapsed) - _t2 - _t1 - _t0) / _t3;
		}
		return 0.0;
	};

	void SetInterval(uint16_t t) {
		_t0 = 0;
		_t1 = t / 3;
		_t2 = t - _t0 - _t0;
		_t3 = _t1;
		_t4 = 0;
		Interval = _t0 + _t1 + _t2 + _t3 + _t4;
	}

	void SetTriangle(uint16_t t, uint16_t delay) {
		_t0 = 0;
		_t1 = t / 2;
		_t2 = 0;
		_t3 = _t1;
		_t4 = delay;
		Interval = _t0 + _t1 + _t2 + _t3 + _t4;
	}

	void SetTriangleCuadrature(uint16_t t, uint16_t delay) {
		_t0 = delay;
		_t1 = t / 2;
		_t2 = 0;
		_t3 = _t1;
		_t4 = 0;
		Interval = _t0 + _t1 + _t2 + _t3 + _t4;
	}
	
	void SetPulse(uint16_t t, uint16_t delay) {
		_t0 = 0;
		_t1 = t / 3;
		_t2 = t - _t0 - _t0;
		_t3 = _t1;
		_t4 = delay;
		Interval = _t0 + _t1 + _t2 + _t3 + _t4;
	}

	void SetPulseCuadrature(uint16_t t, uint16_t delay) {
		_t0 = delay;
		_t1 = t / 3;
		_t2 = t - _t0 - _t0;
		_t3 = _t1;
		_t4 = 0;
		Interval = _t0 + _t1 + _t2 + _t3 + _t4;
	}

	void SetInterval(uint16_t t0, uint16_t t1, uint16_t t2, uint16_t t3, uint16_t t4) {
		_t0 = t0;
		_t1 = t1;
		_t2 = t2;
		_t3 = t3;
		_t4 = t4;
		Interval = _t0 + _t1 + _t2 + _t3 + _t4;
	}

	unsigned long _t0;
	unsigned long _t1;
	unsigned long _t2;
	unsigned long _t3;
	unsigned long _t4;
};

#endif

// ===== EyeConfig.h =====
#ifndef _EYECONFIG_h
#define _EYECONFIG_h

#include <Arduino.h>
struct EyeConfig
{
	int16_t OffsetX;
	int16_t OffsetY;

 	int16_t Height;
	int16_t Width;

	float Slope_Top;
	float Slope_Bottom;

	int16_t Radius_Top;
	int16_t Radius_Bottom;

	int16_t Inverse_Radius_Top;
	int16_t Inverse_Radius_Bottom;

	int16_t Inverse_Offset_Top;
	int16_t Inverse_Offset_Bottom;
};

#endif

// ===== EyeTransition.h =====
#ifndef _EYETRANSITION_h
#define _EYETRANSITION_h

#include <Arduino.h>

class EyeTransition {
public:
	EyeTransition();

	EyeConfig* Origin;
	EyeConfig Destin;

	RampAnimation Animation;

	void Update();
	void Apply(float t);
};

#endif

// ===== EyeTransformation.h =====
#ifndef _EYETRANSFORMATION_h
#define _EYETRANSFORMATION_h

#include <Arduino.h>

struct Transformation
{
	float MoveX = 0.0;
	float MoveY = 0.0;
	float ScaleX = 1.0;
	float ScaleY = 1.0;
};

class EyeTransformation
{
public:
	EyeTransformation();

	EyeConfig* Input;
	EyeConfig Output;

	Transformation Origin;
	Transformation Current;
	Transformation Destin;

	RampAnimation Animation;

	void Update();
	void Apply();
	void SetDestin(Transformation transformation);
};

#endif

// ===== EyeVariation.h =====
#ifndef _EYEVARIATION_h
#define _EYEVARIATION_h

#include <Arduino.h>

class EyeVariation {
public:
	EyeVariation();

	EyeConfig* Input;
	EyeConfig Output;

	TrapeziumPulseAnimation Animation;

	EyeConfig Values;
	void Clear();

	void SetInterval(uint16_t t0, uint16_t t1, uint16_t t2, uint16_t t3, uint16_t t4);

	void Update();
	void Apply(float t);
};

#endif

// ===== EyeBlink.h =====
#ifndef _EYEBLINK_h
#define _EYEBLINK_h

#include <Arduino.h>

class EyeBlink {
 protected:

public:
	EyeBlink();

	EyeConfig* Input;
	EyeConfig Output;

	TrapeziumAnimation Animation;

	int32_t BlinkWidth = 60;
	int32_t BlinkHeight = 2;

	void Update();
	void Apply(float t);
};

#endif

// ===== EyeDrawer.h =====
#ifndef _EYEDRAWER_h
#define _EYEDRAWER_h

#include <Arduino.h>

enum CornerType {T_R, T_L, B_L, B_R};

/**
 * Contains all functions to draw eye based on supplied (expression-based) config
 */
class EyeDrawer {
  public:
    static void Draw(int16_t centerX, int16_t centerY, EyeConfig *config) {
      // Amount by which corners will be shifted up/down based on requested "slope"
      int32_t delta_y_top = config->Height * config->Slope_Top / 2.0;
      int32_t delta_y_bottom = config->Height * config->Slope_Bottom / 2.0;
      // Full extent of the eye, after accounting for slope added at top and bottom
      auto totalHeight = config->Height + delta_y_top - delta_y_bottom;
      // If the requested top/bottom radius would exceed the height of the eye, adjust them downwards 
      if (config->Radius_Bottom > 0 && config->Radius_Top > 0 && totalHeight - 1 < config->Radius_Bottom + config->Radius_Top) {
        int32_t corrected_radius_top = (float)config->Radius_Top * (totalHeight - 1) / (config->Radius_Bottom + config->Radius_Top);
        int32_t corrected_radius_bottom = (float)config->Radius_Bottom * (totalHeight - 1) / (config->Radius_Bottom + config->Radius_Top);
        config->Radius_Top = corrected_radius_top;
        config->Radius_Bottom = corrected_radius_bottom;
      }

      // Calculate _inside_ corners of eye (TL, TR, BL, and BR) before any slope or rounded corners are applied
      int32_t TLc_y = centerY + config->OffsetY - config->Height/2 + config->Radius_Top - delta_y_top;
      int32_t TLc_x = centerX + config->OffsetX - config->Width/2 + config->Radius_Top;
      int32_t TRc_y = centerY + config->OffsetY - config->Height/2 + config->Radius_Top + delta_y_top;
      int32_t TRc_x = centerX + config->OffsetX + config->Width/2 - config->Radius_Top;
      int32_t BLc_y = centerY + config->OffsetY + config->Height/2 - config->Radius_Bottom - delta_y_bottom;
      int32_t BLc_x = centerX + config->OffsetX - config->Width/2 + config->Radius_Bottom;
      int32_t BRc_y = centerY + config->OffsetY + config->Height/2 - config->Radius_Bottom + delta_y_bottom;
      int32_t BRc_x = centerX + config->OffsetX + config->Width/2 - config->Radius_Bottom;
        
      // Calculate interior extents
      int32_t min_c_x = min(TLc_x, BLc_x);
      int32_t max_c_x = max(TRc_x, BRc_x);
      int32_t min_c_y = min(TLc_y, TRc_y);
      int32_t max_c_y = max(BLc_y, BRc_y);

      // Fill eye centre
      EyeDrawer::FillRectangle(min_c_x, min_c_y, max_c_x, max_c_y, 1);

      // Fill eye outwards to meet edges of rounded corners 
      EyeDrawer::FillRectangle(TRc_x, TRc_y, BRc_x + config->Radius_Bottom, BRc_y, 1); // Right
		  EyeDrawer::FillRectangle(TLc_x - config->Radius_Top, TLc_y, BLc_x, BLc_y, 1); // Left
		  EyeDrawer::FillRectangle(TLc_x, TLc_y - config->Radius_Top, TRc_x, TRc_y, 1); // Top
		  EyeDrawer::FillRectangle(BLc_x, BLc_y, BRc_x, BRc_y + config->Radius_Bottom, 1); // Bottom
        
      // Draw slanted edges at top of bottom of eyes 
      // +ve Slope_Top means eyes slope downwards towards middle of face
      if(config->Slope_Top > 0) {
        EyeDrawer::FillRectangularTriangle(TLc_x, TLc_y-config->Radius_Top, TRc_x, TRc_y-config->Radius_Top, 0);
        EyeDrawer::FillRectangularTriangle(TRc_x, TRc_y-config->Radius_Top, TLc_x, TLc_y-config->Radius_Top, 1);
      } 
      else if(config->Slope_Top < 0) {
        EyeDrawer::FillRectangularTriangle(TRc_x, TRc_y-config->Radius_Top, TLc_x, TLc_y-config->Radius_Top, 0);
        EyeDrawer::FillRectangularTriangle(TLc_x, TLc_y-config->Radius_Top, TRc_x, TRc_y-config->Radius_Top, 1);
      }
      // Draw slanted edges at bottom of eyes
      if(config->Slope_Bottom > 0) {
        EyeDrawer::FillRectangularTriangle(BRc_x+config->Radius_Bottom, BRc_y+config->Radius_Bottom, BLc_x-config->Radius_Bottom, BLc_y+config->Radius_Bottom, 0);
        EyeDrawer::FillRectangularTriangle(BLc_x-config->Radius_Bottom, BLc_y+config->Radius_Bottom, BRc_x+config->Radius_Bottom, BRc_y+config->Radius_Bottom, 1);
      }
      else if (config->Slope_Bottom < 0) {
        EyeDrawer::FillRectangularTriangle(BLc_x-config->Radius_Bottom, BLc_y+config->Radius_Bottom, BRc_x+config->Radius_Bottom, BRc_y+config->Radius_Bottom, 0);
        EyeDrawer::FillRectangularTriangle(BRc_x+config->Radius_Bottom, BRc_y+config->Radius_Bottom, BLc_x-config->Radius_Bottom, BLc_y+config->Radius_Bottom, 1);
      }

      // Draw corners (which extend "outwards" towards corner of screen from supplied coordinate values)
      if(config->Radius_Top > 0) {
        EyeDrawer::FillEllipseCorner(T_L, TLc_x, TLc_y, config->Radius_Top, config->Radius_Top, 1);
        EyeDrawer::FillEllipseCorner(T_R, TRc_x, TRc_y, config->Radius_Top, config->Radius_Top, 1);
      }
      if(config->Radius_Bottom > 0) {
        EyeDrawer::FillEllipseCorner(B_L, BLc_x, BLc_y, config->Radius_Bottom, config->Radius_Bottom, 1);
        EyeDrawer::FillEllipseCorner(B_R, BRc_x, BRc_y, config->Radius_Bottom, config->Radius_Bottom, 1);
      }
    }

    // Draw rounded corners
    static void FillEllipseCorner(CornerType corner, int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color) {
      if (rx < 2) return;
      if (ry < 2) return;
      int32_t x, y;
      int32_t rx2 = rx * rx;
      int32_t ry2 = ry * ry;
      int32_t fx2 = 4 * rx2;
      int32_t fy2 = 4 * ry2;
      int32_t s;

      if (corner == T_R) {
        for(x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry); ry2 * x <= rx2 * y; x++) {
          u8g2.drawHLine(x0, y0 - y, x);
          if(s >= 0) {
            s += fx2 * (1 - y);
            y--;
          }
          s += ry2 * ((4 * x) + 6);
        }         
        for(x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx); rx2 * y <= ry2 * x; y++) {
          u8g2.drawHLine(x0, y0 - y, x);
          if (s >= 0) {
            s += fy2 * (1 - x);
            x--;
          }
          s += rx2 * ((4 * y) + 6);
        }
      }

      else if (corner == B_R) {
        for (x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry); ry2 * x <= rx2 * y; x++) {
          u8g2.drawHLine(x0, y0 + y -1, x);
          if (s >= 0) {
            s += fx2 * (1 - y);
            y--;
          }
          s += ry2 * ((4 * x) + 6);
        }
        for (x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx); rx2 * y <= ry2 * x; y++) {
          u8g2.drawHLine(x0, y0 + y -1, x);
          if (s >= 0) {
            s += fy2 * (1 - x);
            x--;
          }
          s += rx2 * ((4 * y) + 6);
        }
      }

      else if (corner == T_L) {
        for (x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry); ry2 * x <= rx2 * y; x++) {
          u8g2.drawHLine(x0-x, y0 - y, x);
          if (s >= 0) {
            s += fx2 * (1 - y);
            y--;
          }
          s += ry2 * ((4 * x) + 6);
        }
        for (x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx); rx2 * y <= ry2 * x; y++) {
          u8g2.drawHLine(x0-x, y0 - y, x);
          if (s >= 0) {
            s += fy2 * (1 - x);
            x--;
          }
          s += rx2 * ((4 * y) + 6);
        }
      }

      else if (corner == B_L) {
        for (x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry); ry2 * x <= rx2 * y; x++) {
          u8g2.drawHLine(x0-x, y0 + y - 1, x);
          if (s >= 0) {
            s += fx2 * (1 - y);
            y--;
          }
          s += ry2 * ((4 * x) + 6);
        }
        for (x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx); rx2 * y <= ry2 * x; y++) {
          u8g2.drawHLine(x0-x, y0 + y , x);
          if (s >= 0) {
            s += fy2 * (1 - x);
            x--;
          }
          s += rx2 * ((4 * y) + 6);
        }
      }
    }

    // Fill a solid rectangle between specified coordinates
    static void FillRectangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t color) {
      // Always draw from TL->BR
      int32_t l = min(x0, x1);
      int32_t r = max(x0, x1);
      int32_t t = min(y0, y1);
      int32_t b = max(y0, y1);
      int32_t w = r-l;
      int32_t h = b-t; 
      u8g2.setDrawColor(color);
      u8g2.drawBox(l, t, w, h);
      u8g2.setDrawColor(1);
    }

    static void FillRectangularTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t color) {
      u8g2.setDrawColor(color);
      u8g2.drawTriangle(x0, y0, x1, y1, x1, y0);
      u8g2.setDrawColor(1);
    }

    static void FillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t color) {
        u8g2.setDrawColor(color);
        u8g2.drawTriangle(x0, y0, x1, y1, x2, y2);
        u8g2.setDrawColor(1);
    }
};

#endif

// ===== FaceEmotions.hpp =====
#ifndef _FACEEMOTIONS_h
#define _FACEEMOTIONS_h

#include <Arduino.h>

enum eEmotions {
	Normal=0,
	Angry,
	Glee,
	Happy,
	Sad,
	Worried,
	Focused,
	Annoyed,
	Surprised,
	Skeptic,
	Frustrated,
	Unimpressed,
	Sleepy,
	Suspicious,
	Squint,
	Furious,
	Scared,
	Awe,
	EMOTIONS_COUNT
};

#endif

// ===== EyePresets.h =====
/**
 * EyePresets.h
 * Defines the eye config associated with each emotion
 */

#ifndef _EYEPRESETS_h
#define _EYEPRESETS_h

#include <Arduino.h>

static const EyeConfig Preset_Normal = {
	.OffsetX = 0,
	.OffsetY = 0,
	.Height = 40,
	.Width = 40,
	.Slope_Top = 0,
	.Slope_Bottom = 0,
	.Radius_Top = 8,
	.Radius_Bottom = 8,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Happy = {
	.OffsetX = 0,
	.OffsetY = 0,
	.Height = 10,
	.Width = 40,
	.Slope_Top = 0,
	.Slope_Bottom = 0,
	.Radius_Top = 10,
	.Radius_Bottom = 0,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Glee = {
	.OffsetX = 0,
	.OffsetY = 0,
	.Height = 8,
	.Width = 40,
	.Slope_Top = 0,
	.Slope_Bottom = 0,
	.Radius_Top = 8,
	.Radius_Bottom = 0,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 5,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Sad = {
	.OffsetX = 0,
	.OffsetY = 0,
	.Height = 15,
	.Width = 40,
	.Slope_Top = -0.5,
	.Slope_Bottom = 0,
	.Radius_Top = 1,
	.Radius_Bottom = 10,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Worried = {
	.OffsetX = 0,
	.OffsetY = 0,
	.Height = 25,
	.Width = 40,
	.Slope_Top = -0.1,
	.Slope_Bottom = 0,
	.Radius_Top = 6,
	.Radius_Bottom = 10,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Worried_Alt = {
	.OffsetX = 0,
	.OffsetY = 0,
	.Height = 35,
	.Width = 40,
	.Slope_Top = -0.2,
	.Slope_Bottom = 0,
	.Radius_Top = 6,
	.Radius_Bottom = 10,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Focused = {
	.OffsetX = 0,
	.OffsetY = 0,
	.Height = 14,
	.Width = 40,
	.Slope_Top = 0.2,
	.Slope_Bottom = 0,
	.Radius_Top = 3,
	.Radius_Bottom = 1,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Annoyed = {
	.OffsetX = 0,
	.OffsetY = 0,
	.Height = 12,
	.Width = 40,
	.Slope_Top = 0,
	.Slope_Bottom = 0,
	.Radius_Top = 0,
	.Radius_Bottom = 10,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Annoyed_Alt = {
	.OffsetX = 0,
	.OffsetY = 0,
	.Height = 5,
	.Width = 40,
	.Slope_Top = 0,
	.Slope_Bottom = 0,
	.Radius_Top = 0,
	.Radius_Bottom = 4,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Surprised = {
	.OffsetX = -2,
	.OffsetY = 0,
	.Height = 45,
	.Width = 45,
	.Slope_Top = 0,
	.Slope_Bottom = 0,
	.Radius_Top = 16,
	.Radius_Bottom = 16,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Skeptic = {
	.OffsetX = 0,
	.OffsetY = 0,
	.Height = 40,
	.Width = 40,
	.Slope_Top = 0,
	.Slope_Bottom = 0,
	.Radius_Top = 10,
	.Radius_Bottom = 10,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Skeptic_Alt = {
	.OffsetX = 0,
	.OffsetY = -6,
	.Height = 26,
	.Width = 40,
	.Slope_Top = 0.3,
	.Slope_Bottom = 0,
	.Radius_Top = 1,
	.Radius_Bottom = 10,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Frustrated = {
	.OffsetX = 3,
	.OffsetY = -5,
	.Height = 12,
	.Width = 40,
	.Slope_Top = 0,
	.Slope_Bottom = 0,
	.Radius_Top = 0,
	.Radius_Bottom = 10,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Unimpressed = {
	.OffsetX = 3,
	.OffsetY = 0,
	.Height = 12,
	.Width = 40,
	.Slope_Top = 0,
	.Slope_Bottom = 0,
	.Radius_Top = 1,
	.Radius_Bottom = 10,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Unimpressed_Alt = {
	.OffsetX = 3,
	.OffsetY = -3,
	.Height = 22,
	.Width = 40,
	.Slope_Top = 0,
	.Slope_Bottom = 0,
	.Radius_Top = 1,
	.Radius_Bottom = 16,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Sleepy = {
	.OffsetX = 0,
	.OffsetY = -2,
	.Height = 14,
	.Width = 40,
	.Slope_Top = -0.5,
	.Slope_Bottom = -0.5,
	.Radius_Top = 3,
	.Radius_Bottom = 3,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Sleepy_Alt = {
	.OffsetX = 0,
	.OffsetY = -2,
	.Height = 8,
	.Width = 40,
	.Slope_Top = -0.5,
	.Slope_Bottom = -0.5,
	.Radius_Top = 3,
	.Radius_Bottom = 3,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Suspicious = {
	.OffsetX = 0,
	.OffsetY = 0,
	.Height = 22,
	.Width = 40,
	.Slope_Top = 0,
	.Slope_Bottom = 0,
	.Radius_Top = 8,
	.Radius_Bottom = 3,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Suspicious_Alt = {
	.OffsetX = 0,
	.OffsetY = -3,
	.Height = 16,
	.Width = 40,
	.Slope_Top = 0.2,
	.Slope_Bottom = 0,
	.Radius_Top = 6,
	.Radius_Bottom = 3,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Squint = {
	.OffsetX = -10,
	.OffsetY = -3,
	.Height = 35,
	.Width = 35,
	.Slope_Top = 0,
	.Slope_Bottom = 0,
	.Radius_Top = 8,
	.Radius_Bottom = 8,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Squint_Alt = {
	.OffsetX = 5,
	.OffsetY = 0,
	.Height = 20,
	.Width = 20,
	.Slope_Top = 0,
	.Slope_Bottom = 0,
	.Radius_Top = 5,
	.Radius_Bottom = 5,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Angry = {
	.OffsetX = -3,
	.OffsetY = 0,
	.Height = 20,
	.Width = 40,
	.Slope_Top = 0.3,
	.Slope_Bottom = 0,
	.Radius_Top = 2,
	.Radius_Bottom = 12,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Furious = {
	.OffsetX = -2,
	.OffsetY = 0,
	.Height = 30,
	.Width = 40,
	.Slope_Top = 0.4,
	.Slope_Bottom = 0,
	.Radius_Top = 2,
	.Radius_Bottom = 8,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Scared = {
	.OffsetX = -3,
	.OffsetY = 0,
	.Height = 40,
	.Width = 40,
	.Slope_Top = -0.1,
	.Slope_Bottom = 0,
	.Radius_Top = 12,
	.Radius_Bottom = 8,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

static const EyeConfig Preset_Awe = {
	.OffsetX = 2,
	.OffsetY = 0,
	.Height = 35,
	.Width = 45,
	.Slope_Top = -0.1,
	.Slope_Bottom = 0.1,
	.Radius_Top = 12,
	.Radius_Bottom = 12,
	.Inverse_Radius_Top = 0,
	.Inverse_Radius_Bottom = 0,
	.Inverse_Offset_Top = 0,
	.Inverse_Offset_Bottom = 0
};

#endif

// ===== BlinkAssistant.h =====
#ifndef _BLINKASSISTANT_h
#define _BLINKASSISTANT_h

#include <Arduino.h>

class Face;

class BlinkAssistant {
 protected:
	Face&  _face;

 public:
	BlinkAssistant(Face& face);

	AsyncTimer Timer;

	void Update();
	void Blink();
};

#endif

// ===== LookAssistant.h =====
#ifndef _LOOKASSISTANT_h
#define _LOOKASSISTANT_h

#include <Arduino.h>

class Face;

class LookAssistant
{
 protected:
	Face&  _face;

 public:
	LookAssistant(Face& face);

	Transformation transformation;

	AsyncTimer Timer;

	void LookAt(float x, float y);
	void Update();
};

#endif

// ===== Eye.h =====
#ifndef _EYE_h
#define _EYE_h

#include <Arduino.h>

class Face;

class Eye {
  protected:
    Face& _face;

    void Update();
    void ChainOperators();

  public:
    Eye(Face& face);

    uint16_t CenterX;
    uint16_t CenterY;
    bool IsMirrored = false;

    EyeConfig Config;
    EyeConfig* FinalConfig;

    EyeTransition Transition;
    EyeTransformation Transformation;
    EyeVariation Variation1;
    EyeVariation Variation2;
    EyeBlink BlinkTransformation;

    void ApplyPreset(const EyeConfig preset);
    void TransitionTo(const EyeConfig preset);
    void Draw();
};

#endif

// ===== FaceExpression.h =====
#ifndef _FACEEXPRESSION_h
#define _FACEEXPRESSION_h

#include <Arduino.h>

class Face;

class FaceExpression {
  protected:
    Face&  _face;

  public:
    FaceExpression(Face& face);

    void ClearVariations();

    void GoTo_Normal();
    void GoTo_Angry();
    void GoTo_Glee();
    void GoTo_Happy();
    void GoTo_Sad();
    void GoTo_Worried();
    void GoTo_Focused();
    void GoTo_Annoyed();
    void GoTo_Surprised();
    void GoTo_Skeptic();
    void GoTo_Frustrated();
    void GoTo_Unimpressed();
    void GoTo_Sleepy();
    void GoTo_Suspicious();
    void GoTo_Squint();
    void GoTo_Furious();
    void GoTo_Scared();
    void GoTo_Awe();
};

#endif

// ===== FaceBehavior.h =====
#ifndef _FACEBEHAVIOR_h
#define _FACEBEHAVIOR_h

#include <Arduino.h>

class Face;

class FaceBehavior
{
 protected:
	Face&  _face;

 public:
	FaceBehavior(Face& face);

	eEmotions CurrentEmotion;

	float Emotions[eEmotions::EMOTIONS_COUNT];

	AsyncTimer Timer;

	void SetEmotion(eEmotions emotion, float value);
	float GetEmotion(eEmotions emotion);

	void Clear();
	void Update();
	eEmotions GetRandomEmotion();

	void GoToEmotion(eEmotions emotion);
};

#endif

// ===== Face.h =====
#ifndef _FACE_h
#define _FACE_h

#include <Arduino.h>

class Face {

public:
    Face(uint16_t screenWidth, uint16_t screenHeight, uint16_t eyeSize);

    uint16_t Width;
    uint16_t Height;
    uint16_t CenterX;
    uint16_t CenterY;
    uint16_t EyeSize;
    uint16_t EyeInterDistance = 4;

    Eye LeftEye;
    Eye RightEye;
    BlinkAssistant Blink;
    LookAssistant Look;
    FaceBehavior Behavior;
    FaceExpression Expression;

    void Update();
    void DoBlink();

    bool RandomBehavior = true;
    bool RandomLook = true;
    bool RandomBlink = true;

    void LookLeft();
    void LookRight();
    void LookFront();
    void LookTop();
    void LookBottom();
    void Wait(unsigned long milliseconds);

protected:
    void Draw();
};

#endif

// =========================== Implementations ===========================


// ===== AsyncTimer.cpp =====
AsyncTimer::AsyncTimer(unsigned long millisInterval) : AsyncTimer(millisInterval, nullptr) {}

AsyncTimer::AsyncTimer(unsigned long millisInterval, AsyncTimerCallback onFinish) {
	Interval = millisInterval;
	OnFinish = onFinish;
}

void AsyncTimer::Start() {
	Reset();
	_isActive = true;
}

void AsyncTimer::Reset() {
	_startTime = millis();
}

void AsyncTimer::Stop() {
	_isActive = false;
}

bool AsyncTimer::Update() {
	if (_isActive == false) return false;

	_isExpired = false;
	if (static_cast<unsigned long>(millis() - _startTime) >= Interval) {
		_isExpired = true;
		if (OnFinish != nullptr) OnFinish();
		Reset();
	}
	return _isExpired;
}

void AsyncTimer::SetIntervalMillis(unsigned long interval) {
	Interval = interval;
}

unsigned long AsyncTimer::GetStartTime() {
	return _startTime;
}

unsigned long AsyncTimer::GetElapsedTime() {
	return millis() - _startTime;
}

unsigned long AsyncTimer::GetRemainingTime() {
	return Interval - millis() + _startTime;
}

bool AsyncTimer::IsActive() const {
	return _isActive;
}

bool AsyncTimer::IsExpired() const{
	return _isExpired;
}

// ===== EyeTransition.cpp =====
EyeTransition::EyeTransition() : Animation(500){}

void EyeTransition::Update() {
	float t = Animation.GetValue();
	Apply(t);
}

void EyeTransition::Apply(float t) {
	Origin->OffsetX = Origin->OffsetX * (1.0 - t) + Destin.OffsetX * t;
	Origin->OffsetY = Origin->OffsetY * (1.0 - t) + Destin.OffsetY * t;
	Origin->Height = Origin->Height * (1.0 - t) + Destin.Height * t;
	Origin->Width = Origin->Width * (1.0 - t) + Destin.Width * t;
	Origin->Slope_Top = Origin->Slope_Top * (1.0 - t) + Destin.Slope_Top * t;
	Origin->Slope_Bottom = Origin->Slope_Bottom * (1.0 - t) + Destin.Slope_Bottom * t;
	Origin->Radius_Top = Origin->Radius_Top * (1.0 - t) + Destin.Radius_Top * t;
	Origin->Radius_Bottom = Origin->Radius_Bottom * (1.0 - t) + Destin.Radius_Bottom * t;
	Origin->Inverse_Radius_Top = Origin->Inverse_Radius_Top * (1.0 - t) + Destin.Inverse_Radius_Top * t;
	Origin->Inverse_Radius_Bottom = Origin->Inverse_Radius_Bottom * (1.0 - t) + Destin.Inverse_Radius_Bottom * t;
	Origin->Inverse_Offset_Top = Origin->Inverse_Offset_Top * (1.0 - t) + Destin.Inverse_Offset_Top * t;
	Origin->Inverse_Offset_Bottom = Origin->Inverse_Offset_Bottom * (1.0 - t) + Destin.Inverse_Offset_Bottom * t;
}

// ===== EyeTransformation.cpp =====
EyeTransformation::EyeTransformation() : Animation(200)
{
}

void EyeTransformation::Update()
{
	auto t = Animation.GetValue();
	Current.MoveX = (Destin.MoveX - Origin.MoveX) * t + Origin.MoveX;
	Current.MoveY = (Destin.MoveY - Origin.MoveY) * t + Origin.MoveY;
	Current.ScaleX = (Destin.ScaleX - Origin.ScaleX) * t + Origin.ScaleX;
	Current.ScaleY = (Destin.ScaleY - Origin.ScaleY) * t + Origin.ScaleY;

	Apply();
}

void EyeTransformation::Apply()
{
	Output.OffsetX = Input->OffsetX + Current.MoveX;
	Output.OffsetY = Input->OffsetY - Current.MoveY;
	Output.Width = Input->Width * Current.ScaleX;
	Output.Height = Input->Height * Current.ScaleY;

	Output.Slope_Top = Input->Slope_Top;
	Output.Slope_Bottom = Input->Slope_Bottom;
	Output.Radius_Top = Input->Radius_Top;
	Output.Radius_Bottom = Input->Radius_Bottom;
	Output.Inverse_Radius_Top = Input->Inverse_Radius_Top;
	Output.Inverse_Radius_Bottom = Input->Inverse_Radius_Bottom;
	Output.Inverse_Offset_Top = Input->Inverse_Offset_Top;
	Output.Inverse_Offset_Bottom = Input->Inverse_Offset_Bottom;
}

void EyeTransformation::SetDestin(Transformation transformation)
{
	Origin.MoveX =  Current.MoveX;
	Origin.MoveY =  Current.MoveY;
	Origin.ScaleX = Current.ScaleX;
	Origin.ScaleY = Current.ScaleY;

	Destin.MoveX =  transformation.MoveX;
	Destin.MoveY =  transformation.MoveY;
	Destin.ScaleX = transformation.ScaleX;
	Destin.ScaleY = transformation.ScaleY;
}

// ===== EyeVariation.cpp =====
EyeVariation::EyeVariation() : Animation(0, 1000, 0, 1000, 0){}

void EyeVariation::Clear() {
	Values.OffsetX = 0;
	Values.OffsetY = 0;
	Values.Height = 0;
	Values.Width = 0;
	Values.Slope_Top = 0;
	Values.Slope_Bottom = 0;
	Values.Radius_Top = 0;
	Values.Radius_Bottom = 0;
	Values.Inverse_Radius_Top = 0;
	Values.Inverse_Radius_Bottom = 0;
	Values.Inverse_Offset_Top = 0;
	Values.Inverse_Offset_Bottom = 0;
}

void EyeVariation::Update() {
	auto t = Animation.GetValue();
	Apply(2.0 * t - 1.0);
}

void EyeVariation::Apply(float t) {
	Output.OffsetX = Input->OffsetX + Values.OffsetX * t;
	Output.OffsetY = Input->OffsetY + Values.OffsetY * t;
	Output.Height = Input->Height + Values.Height * t;;
	Output.Width = Input->Width + Values.Width * t;
	Output.Slope_Top = Input->Slope_Top + Values.Slope_Top * t;
	Output.Slope_Bottom = Input->Slope_Bottom + Values.Slope_Bottom * t;
	Output.Radius_Top = Input->Radius_Top + Values.Radius_Top * t;
	Output.Radius_Bottom = Input->Radius_Bottom + Values.Radius_Bottom * t;
	Output.Inverse_Radius_Top = Input->Inverse_Radius_Top + Values.Inverse_Radius_Top * t;
	Output.Inverse_Radius_Bottom = Input->Inverse_Radius_Bottom + Values.Inverse_Radius_Bottom * t;
	Output.Inverse_Offset_Top = Input->Inverse_Offset_Top + Values.Inverse_Offset_Top * t;
	Output.Inverse_Offset_Bottom = Input->Inverse_Offset_Bottom + Values.Inverse_Offset_Bottom * t;;
}

// ===== EyeBlink.cpp =====
EyeBlink::EyeBlink() : Animation(40, 100, 40) { }

void EyeBlink::Update() {
	auto t = Animation.GetValue();
	if(Animation.GetElapsed() > Animation.Interval) t = 0.0;
	Apply(t * t);
}


void EyeBlink::Apply(float t) {
	Output.OffsetX = Input->OffsetX;
	Output.OffsetY = Input->OffsetY;

	Output.Width = (BlinkWidth - Input->Width) * t + Input->Width;
	Output.Height = (BlinkHeight - Input->Height) * t + Input->Height;

	Output.Slope_Top = Input->Slope_Top * (1.0 - t);
	Output.Slope_Bottom = Input->Slope_Bottom * (1.0 - t);
	Output.Radius_Top = Input->Radius_Top * (1.0 - t);
	Output.Radius_Bottom = Input->Radius_Bottom * (1.0 - t);
	Output.Inverse_Radius_Top = Input->Inverse_Radius_Top * (1.0 - t);
	Output.Inverse_Radius_Bottom = Input->Inverse_Radius_Bottom * (1.0 - t);
	Output.Inverse_Offset_Top = Input->Inverse_Offset_Top * (1.0 - t);
	Output.Inverse_Offset_Bottom = Input->Inverse_Offset_Bottom * (1.0 - t);
}

// ===== BlinkAssistant.cpp =====
BlinkAssistant::BlinkAssistant(Face& face) : _face(face), Timer(3500) {
	Timer.Start();
}

void BlinkAssistant::Update() {
	Timer.Update();

	if (Timer.IsExpired()) {
		Blink();
	}
}

void BlinkAssistant::Blink() {
	_face.LeftEye.BlinkTransformation.Animation.Restart();
	_face.RightEye.BlinkTransformation.Animation.Restart();
	Timer.Reset();
}

// ===== LookAssistant.cpp =====
LookAssistant::LookAssistant(Face& face) : _face(face), Timer(4000)
{
	Timer.Start();
}

void LookAssistant::LookAt(float x, float y)
{
	int16_t moveX_x;
	int16_t moveY_x;
	int16_t moveY_y;
	float scaleY_x;
	float scaleY_y;

  // What is this witchcraft...?!
	moveX_x = -25 * x;
	moveY_x = -3 * x;
	moveY_y = 20 * y;
	scaleY_x = 1.0 - x * 0.2;
	scaleY_y = 1.0 - (y > 0 ? y : -y) * 0.4;

	transformation.MoveX = moveX_x;
	transformation.MoveY = moveY_y; //moveY_x + moveY_y;
	transformation.ScaleX = 1.0;
	transformation.ScaleY = scaleY_x * scaleY_y;
	_face.RightEye.Transformation.SetDestin(transformation);

	moveY_x = +3 * x;
	scaleY_x = 1.0 + x * 0.2;
	transformation.MoveX = moveX_x;
	transformation.MoveY = + moveY_y; //moveY_x + moveY_y;
	transformation.ScaleX = 1.0;
	transformation.ScaleY = scaleY_x * scaleY_y;
	_face.LeftEye.Transformation.SetDestin(transformation);

	_face.RightEye.Transformation.Animation.Restart();
	_face.LeftEye.Transformation.Animation.Restart();
}

void LookAssistant::Update() {
	Timer.Update();

	if (Timer.IsExpired()) {
		Timer.Reset();
		auto x = random(-50, 50);
		auto y = random(-50, 50);
		LookAt((float)x  / 100, (float)y / 100);
	}

}

// ===== Eye.cpp =====
Eye::Eye(Face& face) : _face(face) {

  this->IsMirrored = false;

	ChainOperators();
	Variation1.Animation._t0 = 200;
	Variation1.Animation._t1 = 200;
	Variation1.Animation._t2 = 200;
	Variation1.Animation._t3 = 200;
	Variation1.Animation._t4 = 0;
	Variation1.Animation.Interval = 800;

	Variation2.Animation._t0 = 0;
	Variation2.Animation._t1 = 200;
	Variation2.Animation._t2 = 200;
	Variation2.Animation._t3 = 200;
	Variation2.Animation._t4 = 200;
	Variation2.Animation.Interval = 800;
}

void Eye::ChainOperators() {
	Transition.Origin = &Config;
	Transformation.Input = &Config;
	Variation1.Input = &(Transformation.Output);
	Variation2.Input = &(Variation1.Output);
	BlinkTransformation.Input = &(Variation2.Output);
	FinalConfig = &(BlinkTransformation.Output);
}

void Eye::Update() {
	Transition.Update();
	Transformation.Update();
	Variation1.Update();
	Variation2.Update();
	BlinkTransformation.Update();
}

void Eye::Draw() {
	Update();
	EyeDrawer::Draw(CenterX, CenterY, FinalConfig);
}

void Eye::ApplyPreset(const EyeConfig config) {
	Config.OffsetX = this->IsMirrored ? -config.OffsetX : config.OffsetX;
	Config.OffsetY = -config.OffsetY;
	Config.Height = config.Height;
	Config.Width = config.Width;
	Config.Slope_Top = this->IsMirrored ? config.Slope_Top : -config.Slope_Top;
	Config.Slope_Bottom = this->IsMirrored ? config.Slope_Bottom : -config.Slope_Bottom;
	Config.Radius_Top = config.Radius_Top;
	Config.Radius_Bottom = config.Radius_Bottom;
	Config.Inverse_Radius_Top = config.Inverse_Radius_Top;
	Config.Inverse_Radius_Bottom = config.Inverse_Radius_Bottom;

	Transition.Animation.Restart();
}

void Eye::TransitionTo(const EyeConfig config) {
	Transition.Destin.OffsetX = this->IsMirrored ? -config.OffsetX : config.OffsetX;
	Transition.Destin.OffsetY = -config.OffsetY;
	Transition.Destin.Height = config.Height;
	Transition.Destin.Width = config.Width;
	Transition.Destin.Slope_Top = this->IsMirrored ? config.Slope_Top : -config.Slope_Top;
	Transition.Destin.Slope_Bottom = this->IsMirrored ? config.Slope_Bottom : -config.Slope_Bottom;
	Transition.Destin.Radius_Top = config.Radius_Top;
	Transition.Destin.Radius_Bottom = config.Radius_Bottom;
	Transition.Destin.Inverse_Radius_Top = config.Inverse_Radius_Top;
	Transition.Destin.Inverse_Radius_Bottom = config.Inverse_Radius_Bottom;

	Transition.Animation.Restart();
}

// ===== FaceExpression.cpp =====
FaceExpression::FaceExpression(Face& face) : _face(face)
{
}

void FaceExpression::ClearVariations()
{
	_face.RightEye.Variation1.Clear();
	_face.RightEye.Variation2.Clear();
	_face.LeftEye.Variation1.Clear();
	_face.LeftEye.Variation2.Clear();
	_face.RightEye.Variation1.Animation.Restart();
	_face.LeftEye.Variation1.Animation.Restart();
}

void FaceExpression::GoTo_Normal()
{
	ClearVariations();

	_face.RightEye.Variation1.Values.Height = 3;
	_face.RightEye.Variation2.Values.Width = 1;
	_face.LeftEye.Variation1.Values.Height = 2;
	_face.LeftEye.Variation2.Values.Width = 2;
	_face.RightEye.Variation1.Animation.SetTriangle(1000, 0);
	_face.LeftEye.Variation1.Animation.SetTriangle(1000, 0);

	_face.RightEye.TransitionTo(Preset_Normal);
	_face.LeftEye.TransitionTo(Preset_Normal);

}

void FaceExpression::GoTo_Angry()
{
	ClearVariations();
	_face.RightEye.Variation1.Values.OffsetY = 2;
	_face.LeftEye.Variation1.Values.OffsetY = 2;
	_face.RightEye.Variation1.Animation.SetTriangle(300, 0);
	_face.LeftEye.Variation1.Animation.SetTriangle(300, 0);

	_face.RightEye.TransitionTo(Preset_Angry);
	_face.LeftEye.TransitionTo(Preset_Angry);
}

void FaceExpression::GoTo_Glee()
{
	ClearVariations();
	_face.RightEye.Variation1.Values.OffsetY = 5;
	_face.LeftEye.Variation1.Values.OffsetY = 5;
	_face.RightEye.Variation1.Animation.SetTriangle(300, 0);
	_face.LeftEye.Variation1.Animation.SetTriangle(300, 0);

	_face.RightEye.TransitionTo(Preset_Glee);
	_face.LeftEye.TransitionTo(Preset_Glee);
}

void FaceExpression::GoTo_Happy()
{
	ClearVariations();
	_face.RightEye.TransitionTo(Preset_Happy);
	_face.LeftEye.TransitionTo(Preset_Happy);
}

void FaceExpression::GoTo_Sad()
{
	ClearVariations();
	_face.RightEye.TransitionTo(Preset_Sad);
	_face.LeftEye.TransitionTo(Preset_Sad);
}

void FaceExpression::GoTo_Worried()
{
	ClearVariations();
	_face.RightEye.TransitionTo(Preset_Worried);
	_face.LeftEye.TransitionTo(Preset_Worried_Alt);
}

void FaceExpression::GoTo_Focused()
{
	ClearVariations();
	_face.RightEye.TransitionTo(Preset_Focused);
	_face.LeftEye.TransitionTo(Preset_Focused);
}

void FaceExpression::GoTo_Annoyed()
{
	ClearVariations();
	_face.RightEye.TransitionTo(Preset_Annoyed);
	_face.LeftEye.TransitionTo(Preset_Annoyed_Alt);
}

void FaceExpression::GoTo_Surprised()
{
	ClearVariations();
	_face.RightEye.TransitionTo(Preset_Surprised);
	_face.LeftEye.TransitionTo(Preset_Surprised);
}

void FaceExpression::GoTo_Skeptic()
{
	ClearVariations();
	_face.RightEye.TransitionTo(Preset_Skeptic);
	_face.LeftEye.TransitionTo(Preset_Skeptic_Alt);
}

void FaceExpression::GoTo_Frustrated()
{
	ClearVariations();
	_face.RightEye.TransitionTo(Preset_Frustrated);
	_face.LeftEye.TransitionTo(Preset_Frustrated);
}

void FaceExpression::GoTo_Unimpressed()
{
	ClearVariations();
	_face.RightEye.TransitionTo(Preset_Unimpressed);
	_face.LeftEye.TransitionTo(Preset_Unimpressed_Alt);
}

void FaceExpression::GoTo_Sleepy()
{
	ClearVariations();
	_face.RightEye.TransitionTo(Preset_Sleepy);
	_face.LeftEye.TransitionTo(Preset_Sleepy_Alt);
}

void FaceExpression::GoTo_Suspicious()
{
	ClearVariations();
	_face.RightEye.TransitionTo(Preset_Suspicious);
	_face.LeftEye.TransitionTo(Preset_Suspicious_Alt);
}

void FaceExpression::GoTo_Squint()
{
	ClearVariations();

	_face.LeftEye.Variation1.Values.OffsetX = 6;
	_face.LeftEye.Variation2.Values.OffsetY = 6;

	_face.RightEye.TransitionTo(Preset_Squint);
	_face.LeftEye.TransitionTo(Preset_Squint_Alt);

}

void FaceExpression::GoTo_Furious()
{
	ClearVariations();

	_face.RightEye.TransitionTo(Preset_Furious);
	_face.LeftEye.TransitionTo(Preset_Furious);
}

void FaceExpression::GoTo_Scared()
{
	ClearVariations();

	_face.RightEye.TransitionTo(Preset_Scared);
	_face.LeftEye.TransitionTo(Preset_Scared);
}

void FaceExpression::GoTo_Awe()
{
	ClearVariations();

	_face.RightEye.TransitionTo(Preset_Awe);
	_face.LeftEye.TransitionTo(Preset_Awe);
}

// ===== FaceBehavior.cpp =====
FaceBehavior::FaceBehavior(Face& face) : _face(face), Timer(500) {
	Timer.Start();
	Clear();
	Emotions[(int)eEmotions::Normal] = 1.0;
}

void FaceBehavior::SetEmotion(eEmotions emotion, float value) {
	Emotions[emotion] = value;
}

float FaceBehavior::GetEmotion(eEmotions emotion) {
	return Emotions[emotion];
}

void FaceBehavior::Clear() {
	for (int emotion = 0; emotion < eEmotions::EMOTIONS_COUNT; emotion++) {
		Emotions[emotion] = 0.0;
	}
}

// Use roulette wheel to select a new emotion, based on assigned weights
eEmotions FaceBehavior::GetRandomEmotion() {

  // Calculate the total sum of all emotional weights
	float sum_of_weight = 0;
	for (int emotion = 0; emotion < eEmotions::EMOTIONS_COUNT; emotion++) {
		sum_of_weight += Emotions[emotion];
	}
  // If no weights have been assigned, default to "normal" emotion
	if (sum_of_weight == 0) {
		return eEmotions::Normal;
	}
  // Now pick a random number that lies somewhere in the range of total weights
	float rand = random(0, 1000 * sum_of_weight) / 1000.0;
  // Loop over emotions and select the one whose probabity distribution contains
  // the value in which the random number lies
	float acc = 0;
	for (int emotion = 0; emotion < eEmotions::EMOTIONS_COUNT; emotion++) {
		if (Emotions[emotion] == 0) continue;
		acc += Emotions[emotion];
		if (rand <= acc) {
			return (eEmotions)emotion;
		}
	}
  // If something goes wrong in the calculation, return "normal"
	return eEmotions::Normal;
}

void FaceBehavior::Update() {
	Timer.Update();

	if (Timer.IsExpired()) {
		Timer.Reset();
		eEmotions newEmotion = GetRandomEmotion();
		if (CurrentEmotion != newEmotion) {
			GoToEmotion(newEmotion);
		}
	}
}

void FaceBehavior::GoToEmotion(eEmotions emotion) {
  // Set the currentEmotion to the desired emotion
	CurrentEmotion = emotion;

  // Call the appropriate expression transition function 
	switch (CurrentEmotion) {
    case eEmotions::Normal: _face.Expression.GoTo_Normal(); break;
    case eEmotions::Angry: _face.Expression.GoTo_Angry(); break;
    case eEmotions::Glee: _face.Expression.GoTo_Glee(); break;
    case eEmotions::Happy: _face.Expression.GoTo_Happy(); break;
    case eEmotions::Sad: _face.Expression.GoTo_Sad(); break;
    case eEmotions::Worried: _face.Expression.GoTo_Worried(); break;
    case eEmotions::Focused: _face.Expression.GoTo_Focused(); break;
    case eEmotions::Annoyed: _face.Expression.GoTo_Annoyed(); break;
    case eEmotions::Surprised: _face.Expression.GoTo_Surprised(); break;
    case eEmotions::Skeptic: _face.Expression.GoTo_Skeptic(); break;
    case eEmotions::Frustrated: _face.Expression.GoTo_Frustrated(); break;
    case eEmotions::Unimpressed: _face.Expression.GoTo_Unimpressed(); break;
    case eEmotions::Sleepy: _face.Expression.GoTo_Sleepy(); break;
    case eEmotions::Suspicious: _face.Expression.GoTo_Suspicious(); break;
    case eEmotions::Squint: _face.Expression.GoTo_Squint(); break;
    case eEmotions::Furious: _face.Expression.GoTo_Furious(); break;
    case eEmotions::Scared: _face.Expression.GoTo_Scared(); break;
    case eEmotions::Awe: _face.Expression.GoTo_Awe(); break;
    default: break;
	}
}

// ===== Face.cpp =====
Face::Face(uint16_t screenWidth, uint16_t screenHeight, uint16_t eyeSize) 
	: LeftEye(*this), RightEye(*this), Blink(*this), Look(*this), Behavior(*this), Expression(*this) {

  // Unlike almost every other Arduino library (and the I2C address scanner script etc.)
  // u8g2 uses 8-bit I2C address, so we shift the 7-bit address left by one

	Width = screenWidth;
	Height = screenHeight;
	EyeSize = eyeSize;

	CenterX = Width / 2;
	CenterY = Height / 2;

	LeftEye.IsMirrored = true;

  Behavior.Clear();
	Behavior.Timer.Start();
}

void Face::LookFront() {
	Look.LookAt(0.0, 0.0);
}

void Face::LookRight() {
	Look.LookAt(-1.0, 0.0);
}

void Face::LookLeft() {
	Look.LookAt(1.0, 0.0);
}

void Face::LookTop() {
	Look.LookAt(0.0, 1.0);
}

void Face::LookBottom() {
	Look.LookAt(0.0, -1.0);
}

void Face::Wait(unsigned long milliseconds) {
	unsigned long start;
	start = millis();
	while (millis() - start < milliseconds) {
		Draw();
	}
}

void Face::DoBlink() {
	Blink.Blink();
}

void Face::Update() {
	if(RandomBehavior) Behavior.Update();
	if(RandomLook) Look.Update();
	if(RandomBlink)	Blink.Update();
	Draw();
}

void Face::Draw() {
  u8g2.clearBuffer();     // ✅ 必须：清空上一帧/随机垃圾
  u8g2.setDrawColor(1);   // ✅ 建议：确保用“点亮像素”绘制

  LeftEye.CenterX = CenterX - EyeSize / 2 - EyeInterDistance;
  LeftEye.CenterY = CenterY;
  LeftEye.Draw();

  RightEye.CenterX = CenterX + EyeSize / 2 + EyeInterDistance;
  RightEye.CenterY = CenterY;
  RightEye.Draw();

  u8g2.sendBuffer();
}


// =========================== App code ===========================

// =========================== App code ===========================

static Face* gFace = nullptr;
static WebServer server(80);
static uint32_t lastFrameMs = 0;

static bool parseEmotion(const String& s, eEmotions &out) {
  String t = s; t.trim();
  if (t.length()==0) return false;
  // allow numeric
  bool isNum = true;
  for (size_t i=0;i<t.length();i++){ if (!isDigit(t[i])) { isNum=false; break; } }
  if (isNum) {
    int v = t.toInt();
    if (v>=0 && v < (int)eEmotions::EMOTIONS_COUNT) { out = (eEmotions)v; return true; }
    return false;
  }
  t.toLowerCase();
  struct Pair{ const char* name; eEmotions e; };
  static const Pair map[] = {
    {"normal", eEmotions::Normal}, {"angry", eEmotions::Angry}, {"glee", eEmotions::Glee},
    {"happy", eEmotions::Happy}, {"sad", eEmotions::Sad}, {"worried", eEmotions::Worried},
    {"focused", eEmotions::Focused}, {"annoyed", eEmotions::Annoyed}, {"surprised", eEmotions::Surprised},
    {"skeptic", eEmotions::Skeptic}, {"frustrated", eEmotions::Frustrated}, {"unimpressed", eEmotions::Unimpressed},
    {"sleepy", eEmotions::Sleepy}, {"suspicious", eEmotions::Suspicious}, {"squint", eEmotions::Squint},
    {"furious", eEmotions::Furious}, {"scared", eEmotions::Scared}, {"awe", eEmotions::Awe},
  };
  for (auto &p: map) { if (t == p.name) { out = p.e; return true; } }
  return false;
}

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>ESP32 Eyes</title>
<style>
  body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial; margin:16px; max-width:760px}
  .grid{display:grid; grid-template-columns:repeat(3,1fr); gap:10px}
  button{padding:10px 12px; border:1px solid #ddd; border-radius:10px; background:#f7f7f7}
  button:active{transform:scale(0.99)}
  .row{display:flex; gap:10px; flex-wrap:wrap; margin:12px 0}
  .card{border:1px solid #eee; border-radius:14px; padding:12px; margin:12px 0}
  input[type=range]{width:240px}
  code{background:#f3f3f3; padding:2px 6px; border-radius:6px}
</style>
</head>
<body>
<h2>ESP32 OLED Eyes</h2>
<p>Connect to Wi‑Fi: <code>ESP32-EYES</code> then open <code>http://192.168.4.1</code></p>

<div class="card">
  <h3>Emotion</h3>
  <div id="emo" class="grid"></div>
</div>

<div class="card">
  <h3>Manual control</h3>
  <div class="row">
    <button onclick="blink()">Blink</button>
    <button onclick="autoAll()">Auto Look/Blink/Behavior</button>
    <button onclick="stopAuto()">Manual Mode</button>
  </div>

  <div class="row">
    <label>Look X <span id="lxv">0</span><br/>
      <input id="lx" type="range" min="-100" max="100" value="0" oninput="setLook()"/>
    </label>
    <label>Look Y <span id="lyv">0</span><br/>
      <input id="ly" type="range" min="-100" max="100" value="0" oninput="setLook()"/>
    </label>
  </div>
</div>

<div class="card">
  <h3>Status</h3>
  <pre id="st">...</pre>
</div>

<script>
const EMOS = [
  "Normal","Angry","Glee","Happy","Sad","Worried","Focused","Annoyed",
  "Surprised","Skeptic","Frustrated","Unimpressed","Sleepy","Suspicious",
  "Squint","Furious","Scared","Awe"
];

function q(url){ return fetch(url).then(r=>r.text()); }

function buildButtons(){
  const root = document.getElementById('emo');
  EMOS.forEach((name,i)=>{
    const b=document.createElement('button');
    b.textContent = name;
    b.onclick=()=> setEmotion(name);
    root.appendChild(b);
  });
}

function setEmotion(name){
  q('/api?emo='+encodeURIComponent(name)).then(updateStatus);
}
function blink(){ q('/api?blink=1').then(updateStatus); }

function setLook(){
  const lx = document.getElementById('lx').value;
  const ly = document.getElementById('ly').value;
  document.getElementById('lxv').textContent = (lx/100).toFixed(2);
  document.getElementById('lyv').textContent = (ly/100).toFixed(2);
  q(`/api?lookx=${lx/100}&looky=${ly/100}`).then(updateStatus);
}
function autoAll(){ q('/api?auto=1').then(updateStatus); }
function stopAuto(){ q('/api?auto=0').then(updateStatus); }

function updateStatus(t){
  if (typeof t === 'string') document.getElementById('st').textContent = t;
}

buildButtons();
q('/api').then(updateStatus);
setInterval(()=>q('/api').then(updateStatus), 1200);
</script>
</body>
</html>
)HTML";


static String statusJson() {
  if (!gFace) return "{\"ok\":false}";
  // Note: Behavior.CurrentEmotion tracks last selected emotion
  String s = "{";
  s += "\"ok\":true";
  s += ",\"emotion\":" + String((int)gFace->Behavior.CurrentEmotion);
  s += ",\"randomBehavior\":" + String(gFace->RandomBehavior ? "true":"false");
  s += ",\"randomLook\":" + String(gFace->RandomLook ? "true":"false");
  s += ",\"randomBlink\":" + String(gFace->RandomBlink ? "true":"false");
  s += "}";
  return s;
}

static void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

static void handleApi() {
  if (!gFace) { server.send(500, "text/plain", "face not ready"); return; }

  // Auto mode toggle
  if (server.hasArg("auto")) {
    int on = server.arg("auto").toInt();
    gFace->RandomBehavior = (on != 0);
    gFace->RandomLook = (on != 0);
    gFace->RandomBlink = (on != 0);
  }

  // Manual blink
  if (server.hasArg("blink")) {
    gFace->DoBlink();
  }

  // Set emotion (by name or index)
  if (server.hasArg("emo")) {
    eEmotions e;
    if (parseEmotion(server.arg("emo"), e)) {
      gFace->RandomBehavior = false; // manual emotion implies manual mode
      gFace->Behavior.GoToEmotion(e);
    }
  }

  // Manual look
  if (server.hasArg("lookx") || server.hasArg("looky")) {
    float x = server.hasArg("lookx") ? server.arg("lookx").toFloat() : 0;
    float y = server.hasArg("looky") ? server.arg("looky").toFloat() : 0;
    if (x < -1) x = -1; if (x > 1) x = 1;
    if (y < -1) y = -1; if (y > 1) y = 1;
    gFace->RandomLook = false;
    gFace->Look.LookAt(x, y);
  }

  server.send(200, "application/json", statusJson());
}

static void wifiStartAP() {
  WiFi.mode(WIFI_AP);
  // Default softAP IP is 192.168.4.1; keep default for simplicity
  bool ok = WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("SoftAP %s: %s\n", ok ? "started":"FAILED", AP_SSID);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
}

void setup() {
  Serial.begin(115200);
  delay(50);

  // OLED init (HW I2C)
  Wire.begin(OLED_SDA, OLED_SCL);
  u8g2.setI2CAddress(OLED_ADDR_7BIT << 1);
u8g2.begin();
u8g2.clearBuffer();
u8g2.sendBuffer();
  u8g2.setBusClock(400000);
  u8g2.clearBuffer();
  u8g2.sendBuffer();

  // Create face AFTER OLED is ready
  gFace = new Face(128, 64, 40);  // eyeSize can be tuned: 34~44 typical
  gFace->RandomBehavior = false;  // start manual (you can change)
  gFace->RandomLook = true;
  gFace->RandomBlink = true;
  gFace->Behavior.GoToEmotion(eEmotions::Normal);

  // WiFi + Web
  wifiStartAP();
  server.on("/", handleRoot);
  server.on("/api", handleApi);
  server.begin();
  Serial.println("Open: http://192.168.4.1/");
}

void loop() {
  server.handleClient();

  // Throttle draw/update
  uint32_t now = millis();
  if (gFace && (uint32_t)(now - lastFrameMs) >= EYES_FRAME_MS) {
    gFace->Update();
    lastFrameMs = now;
  }
}
