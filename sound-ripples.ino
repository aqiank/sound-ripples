#include <SoftPWM.h>
#include <EEPROM.h>

//#define PLAY_FROM_EEPROM
#define DEBUG

static const int SAMPLE_WINDOW = 50; // Sample window width in mS (50 mS = 20Hz)

static const int BUTTON_PIN = 13;

static const int MOSS_START_PIN = 0;
static const int MOSS_END_PIN = 2;

static const int FLOWERS_START_PIN = 3;
static const int FLOWERS_END_PIN = 12;

static const int EEPROM_HEADER_LENGTH = 10;
static const int BUF_LEN = (2048 - EEPROM_HEADER_LENGTH); // Change number to match EEPROM size
static const uint8_t MAGIC[] = { 0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF };

static uint8_t audioLevels[BUF_LEN];
static int recordIndex;
static int playIndex;
static bool bIsButtonStillPressed;
static bool bHasPlayedOnce;

void setup() {
#ifdef DEBUG
	Serial.begin(9600);
#endif

#ifdef PLAY_FROM_EEPROM
	load();
#else
	pinMode(BUTTON_PIN, OUTPUT);
	digitalWrite(BUTTON_PIN, HIGH);
#endif

	SoftPWMBegin(SOFTPWM_NORMAL);

	for (int i = MOSS_START_PIN; i <= MOSS_END_PIN; i++) {
		pinMode(i, OUTPUT);
	}
	
	for (int i = FLOWERS_START_PIN; i <= FLOWERS_END_PIN; i++) {
		pinMode(i, OUTPUT);
	}
}
 
void loop() {
#ifdef PLAY_FROM_EEPROM
	const uint8_t level = audioLevels[playIndex];

	setMossBrightness(level);
	setFlowersBrightness(level);

#ifdef DEBUG
	Serial.print("play: ");
	Serial.println(level);
#endif

	playIndex = (playIndex + 1) % (recordIndex + 1);

	delay(SAMPLE_WINDOW);
#else
	// Get audio level from mic
	const uint8_t level = getAudioLevel();

	// Check if button is pressed
	bool bIsButtonPressed = !digitalRead(BUTTON_PIN);

	// Do stuff as button change state
	if (bIsButtonPressed != bIsButtonStillPressed) {
#ifdef DEBUG
		Serial.println(bIsButtonPressed ? "pressed" : "released");
#endif
		onButtonChangeState(bIsButtonPressed);
	}

	// Record or playback (depending on the button state)
	recordOrPlayback(level);
#endif
}

uint8_t getAudioLevel() {
	unsigned long startMillis = millis();   // Start of sample window
	unsigned int peakToPeak = 0;            // peak-to-peak level
 
	unsigned int signalMax = 0;
	unsigned int signalMin = 1024;
 
	// collect data for 50 mS
	while (millis() - startMillis < SAMPLE_WINDOW) {
		unsigned int sample = analogRead(A0);
		
		// toss out spurious readings 
		if (sample < 1024) {
			if (sample > signalMax) {
				signalMax = sample;	// save just the max levels
			} else if (sample < signalMin) {
				signalMin = sample;	// save just the min levels
			}
		}
	}

	peakToPeak = signalMax - signalMin;	// max - min = peak-peak amplitude
	return (uint8_t) map(peakToPeak, 0, 1024, 0, 255);
}

void onButtonChangeState(bool bIsButtonPressed) {
	bIsButtonStillPressed = bIsButtonPressed;
	if (bIsButtonPressed) {
		setFlowersBrightness(0);
		recordIndex = 0;
	} else {
		save();
		setMossBrightness(0);
		bHasPlayedOnce = false;
		playIndex = 0;
	}
}

void recordOrPlayback(uint8_t level) {
	if (bIsButtonStillPressed) {
		if (recordIndex < BUF_LEN) {
			audioLevels[recordIndex++] = level;
			setMossBrightness(level);
#ifdef DEBUG
			Serial.print("record: ");
			Serial.println(level);
#endif
		}
	} else {
		if (recordIndex > 0) {
			const uint8_t level = audioLevels[playIndex];
			setMossBrightness(level);
			if (bHasPlayedOnce) {
				setFlowersBrightness(level);
			}
			playIndex = (playIndex + 1) % (recordIndex + 1);
			if (playIndex == 0) {
				bHasPlayedOnce = true;
			}
#ifdef DEBUG
			Serial.print("play: ");
			Serial.println(level);
#endif
		}
	}
}

void setMossBrightness(uint8_t level) {
	for (int i = MOSS_START_PIN; i <= MOSS_END_PIN; i++) {
		SoftPWMSet(i, level);
	}
}

void setFlowersBrightness(uint8_t level) {
	for (int i = FLOWERS_START_PIN; i <= FLOWERS_END_PIN; i++) {
		SoftPWMSet(i, level);
	}
}

void save() {
	for (size_t i = 0; i < sizeof(MAGIC); i++) {
		EEPROM.write(i, MAGIC[i]);
	}

	EEPROM.write(8, recordIndex & 0xFF);
	EEPROM.write(9, (recordIndex >> 8) & 0xFF);

	for (int i = 0; i < recordIndex; i++) {
		EEPROM.write(EEPROM_HEADER_LENGTH + recordIndex, audioLevels[i]);
	}

#ifdef DEBUG
	Serial.print("saved ");
	Serial.print(recordIndex);
	Serial.println(" bytes");
#endif
}

void load() {
	if (hasEEPROMData()) {
		recordIndex = EEPROM.read(8) | EEPROM.read(9) << 8;
		for (int i = 0; i < recordIndex; i++) {
			audioLevels[i] = EEPROM.read(EEPROM_HEADER_LENGTH + i);
		}
#ifdef DEBUG
		Serial.print("loaded ");
		Serial.print(recordIndex);
		Serial.println(" bytes");
#endif
	} else {
#ifdef DEBUG
		Serial.println("No audio data was saved");
#endif
	}
}

bool hasEEPROMData() {
	for (size_t i = 0; i < sizeof(MAGIC); i++) {
		if (EEPROM.read(i) != MAGIC[i]) {
			return false;
		}
	}
	return true;
}
