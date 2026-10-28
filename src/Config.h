#ifndef SOFAR_CONFIG_H
#define SOFAR_CONFIG_H

// Umbrella for firmware sources. Pure headers and native tests include the
// dependency-free parts directly instead of this (it pulls in Arduino.h).
#include <Arduino.h>
#include "hw/Pins.h"
#include "inverter/Registers.h"
#include "Tuning.h"
#include "config/EELayout.h"

#endif
