#include <FastLED.h>

#define LED_TYPE     WS2812B
#define COLOR_ORDER  GRB

// Пины лент (пока используем только первую)
#define PIN_STRIP_0  0
// #define PIN_STRIP_1  5
// #define PIN_STRIP_2  6

// Размеры буферов под каждую ленту
constexpr uint16_t NUM_LEDS_0 = 10;
// constexpr uint16_t NUM_LEDS_1 = 16;
// constexpr uint16_t NUM_LEDS_2 = 16;

CRGB leds0[NUM_LEDS_0];
// CRGB leds1[NUM_LEDS_1];
// CRGB leds2[NUM_LEDS_2];

enum StripId : uint8_t {
  STRIP_0 = 0,
  STRIP_1 = 1,
  STRIP_2 = 2
};

enum EffectType : uint8_t {
  EFF_WARM_STATIC,
  EFF_FIREPLACE,
  EFF_LAB_SPARKS,
  EFF_POTION_SWAMP // Болотное свечение котла / лаборатории зельеварения
};

struct StripDescriptor {
  CRGB* leds;
  uint16_t count;
  bool enabled;
};

struct Zone {
  StripId strip;
  uint16_t start;
  uint16_t count;
  EffectType effect;
  bool enabled;
};

// Таблица подключённых лент
StripDescriptor strips[] = {
  { leds0, NUM_LEDS_0, true  },
  // { leds1, NUM_LEDS_1, false },
  // { leds2, NUM_LEDS_2, false },
};
constexpr size_t NUM_STRIPS = sizeof(strips) / sizeof(strips[0]);

// Таблица участков — при добавлении ленты просто указываем нужный StripId
Zone zones[] = {
  { STRIP_0, 0, 2, EFF_WARM_STATIC,  true }, // 1-й этаж «Трёх мётел»
  { STRIP_0, 2, 2, EFF_POTION_SWAMP, true },
  { STRIP_0, 4, 2, EFF_WARM_STATIC,  true }, // 2-й этаж «Трёх мётел»
  { STRIP_0, 6, 2, EFF_FIREPLACE,    true }, // Камин
  { STRIP_0, 8, 1, EFF_LAB_SPARKS,   true }, // Вспышки в лаборатории
  { STRIP_0, 9, 1, EFF_WARM_STATIC,  true }, // Прочие фонари/окна

  // Заготовки под будущие ленты (пока выключены):
  // { STRIP_1, 0, 4, EFF_WARM_STATIC, false },
  // { STRIP_1, 4, 2, EFF_FIREPLACE,   false },
};
constexpr size_t NUM_ZONES = sizeof(zones) / sizeof(zones[0]);

void renderZone(const Zone& z, uint32_t ms) {
  if (!z.enabled || z.strip >= NUM_STRIPS) return;

  const StripDescriptor& s = strips[z.strip];
  if (!s.enabled || s.leds == nullptr) return;
  if (z.start >= s.count || z.count == 0) return;

  // Защита от выхода за пределы буфера ленты
  uint16_t safeCount = z.count;
  if (z.start + safeCount > s.count) {
    safeCount = s.count - z.start;
  }

  CRGB* slice = s.leds + z.start;

  switch (z.effect) {
    case EFF_WARM_STATIC: {
      // Мягкий тёплый свет вокруг базового CRGB(50, 40, 10) с лёгким живым колебанием
      for (uint16_t i = 0; i < safeCount; i++) {
        uint8_t n = inoise8(ms * 4 + i * 29 + z.start * 13 + z.strip * 97);
        // Лёгкое колебание масштаба в пределах ~85..100% от заданного цвета
        uint8_t warmScale = map(n, 0, 255, 100, 150);
        slice[i] = CRGB(100, 40, 10);
        slice[i].nscale8(warmScale);
      }
      break;
    }
    case EFF_FIREPLACE: {
      for (uint16_t i = 0; i < safeCount; i++) {
        uint8_t n = inoise8(ms * 20 + i * 53 + z.start * 17 + z.strip * 97);
        uint8_t greenVal = 25 + scale8(n, 45);
        uint8_t scale = map(n, 0, 255, 10, 20);
        slice[i] = CRGB(255, greenVal, 0);
        slice[i].nscale8(scale);
      }
      break;
    }

    case EFF_LAB_SPARKS: {
      uint8_t r = random8();
      if (r < 12) {
        fill_solid(slice, safeCount, CRGB(120, 200, 255));
      } else {
        fadeToBlackBy(slice, safeCount, 60);
      }
      break;
    }

    case EFF_POTION_SWAMP: {
      // Очень медленное мерцание зелья: ленивый дрейф оттенка + плавное колебание яркости
      uint32_t slowTime = ms >> 2; // В 4 раза реже меняется аргумент шума, без переполнения ms * N
      for (uint16_t i = 0; i < safeCount; i++) {
        uint32_t phase = i * 73 + z.start * 29 + z.strip * 97 + 411;

        // Ещё более ленивый дрейф оттенка
        uint8_t colorNoise = inoise8((slowTime >> 1) + phase);
        // Медленное мерцание яркости (~в 3–4 раза медленнее предыдущего варианта)
        uint8_t flickerNoise = inoise8(slowTime + phase + 1907);

        // Болотно-зелёная палитра без «кислоты»
        uint8_t redVal   = 12 + scale8(colorNoise, 33);
        uint8_t greenVal = 110 + scale8(colorNoise, 130);
        uint8_t blueVal  = scale8(colorNoise, 18);

        // Плавное медленное колебание яркости
        uint8_t swampScale = map(flickerNoise, 0, 255, 20, 85);

        slice[i] = CRGB(redVal, greenVal, blueVal);
        slice[i].nscale8(swampScale);
      }
      break;
    }  
  }
}

void setup() {
  FastLED.addLeds<LED_TYPE, PIN_STRIP_0, COLOR_ORDER>(leds0, NUM_LEDS_0);
  // FastLED.addLeds<LED_TYPE, PIN_STRIP_1, COLOR_ORDER>(leds1, NUM_LEDS_1);
  // FastLED.addLeds<LED_TYPE, PIN_STRIP_2, COLOR_ORDER>(leds2, NUM_LEDS_2);

  FastLED.setBrightness(255);
}

void loop() {
  uint32_t ms = millis();

  for (size_t i = 0; i < NUM_ZONES; i++) {
    renderZone(zones[i], ms);
  }

  FastLED.show();
  delay(25);
}