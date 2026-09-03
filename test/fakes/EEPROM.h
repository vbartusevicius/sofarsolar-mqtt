#ifndef FAKE_EEPROM_H
#define FAKE_EEPROM_H

#include <cstdint>
#include <vector>

// Fake EEPROM: bytes start erased (0xFF); write()/commit() calls are counted
// so tests can assert the firmware's dirty-guard behaviour.
class FakeEEPROMClass {
public:
    std::vector<uint8_t> mem;
    uint32_t writeCalls  = 0;   // EEPROM.write() calls actually made
    uint32_t commitCalls = 0;   // EEPROM.commit() calls made

    void begin(size_t size) {
        mem.assign(size, 0xFF);
        writeCalls = commitCalls = 0;
    }
    uint8_t read(int addr) const          { return mem[addr]; }
    void write(int addr, uint8_t v)       { writeCalls++; mem[addr] = v; }
    bool commit()                         { commitCalls++; return true; }
};

inline FakeEEPROMClass EEPROM;

#endif
