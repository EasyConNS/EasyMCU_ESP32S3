#include "easycon.h"
#include "hid_device.h"
#include <string.h>
#include <stdlib.h>

static uint8_t mem[MEM_SIZE];
static uint8_t serial_buffer[SERIAL_BUFFER_SIZE];
static size_t serial_buffer_length = 0;
static bool serial_command_ready = true;
static uint16_t flash_addr = 0;
static uint16_t flash_index = 0;
static uint16_t flash_count = 0;
static uint16_t script_addr = 0;
static uint16_t script_eof = 0;
static uint16_t tail_wait = 0;
static uint32_t timer_elapsed = 0;
static bool auto_run = false;
static volatile uint8_t echo_ms = 0;
static volatile uint8_t _report_echo = 0;
static volatile uint8_t _script_running = 0;
static volatile uint8_t _ledflag = 0;
static volatile uint32_t timer_ms = 0;
static volatile uint32_t wait_ms = 0;

static uint8_t _ins0, _ins1, _ins2, _ins3;
static uint16_t _ins_word;
static uint32_t _ins_dword;
static uint16_t _addr;
static uint8_t _keycode, _lr, _direction;
static int32_t _forstackindex = 0;
static int32_t _callstackindex = 0;
static int32_t _stackindex = 0;
static uint8_t _e_val = 0;
static int16_t _ri0, _ri1;
static bool _v = false, _flag = false;
static uint16_t _seed = 0;

#define DX(i) (mem[20 + (i) * 2])
#define DY(i) (mem[21 + (i) * 2])
#define KEY(i) (mem[90 + (i)])
#define REG(i) (*(int16_t *)(mem + 130 + ((i) - 1) * 2))
#define STACK(i) (mem[180 + (i)])
#define CALLSTACK(i) (*(int16_t *)(mem + 230 + (i) * 2))
#define FOR_I(i) (*(int32_t *)(mem + 290 + (i) * 16))
#define FOR_ADDR(i) (*(uint16_t *)(mem + 294 + (i) * 16))
#define FOR_NEXT(i) (*(uint16_t *)(mem + 296 + (i) * 16))
#define FOR_C(i) (*(int32_t *)(mem + 298 + (i) * 16))
#define KEYCODE_MAX 33
#define VARSPACE_OFFSET 130
#define SEED_OFFSET (MEM_SIZE)
#define LED_SETTING (MEM_SIZE + 2)

#define E_SET (_e_val != 0)
#define E(v) (_e_val = (v))
#define _ins (_ins_word)
#define _insEx (_ins_dword)
#define _BV(i) (1 << (i))
#define Min(a, b) ((a) < (b) ? (a) : (b))
#define Max(a, b) ((a) > (b) ? (a) : (b))
#define SETWAIT(n) do { wait_ms = (uint32_t)(n); } while(0)
#define JUMP(addr) do { script_addr = (uint16_t)(addr); } while(0)
#define JUMPNEAR(offset) do { script_addr = (uint16_t)(script_addr + (offset)); } while(0)
#define RESETAFTER(keycode, n) do { KEY(keycode) = (n); } while(0)

static void binaryop(uint8_t op, uint8_t reg, int16_t value);

void EasyCon_serial_send(uint8_t byte) {
    extern void uart_handler_send(uint8_t);
    uart_handler_send(byte);
}

void EasyCon_script_init(void) {
    uint8_t mem0 = EasyCon_read_byte(0);
    uint8_t mem1 = EasyCon_read_byte(1);
    uint16_t len = 0;
    if ((mem0 != 0xFF) || (mem1 != 0xFF)) {
        len = mem0 | ((mem1 & 0x7F) << 8);
        EasyCon_write_start(0);
        EasyCon_write_data(0, mem, len);
        _seed = (uint16_t)EasyCon_read_byte(SEED_OFFSET + 1) << 8 | EasyCon_read_byte(SEED_OFFSET);
        _seed += 1;
        srand(_seed);
        EasyCon_write_data(SEED_OFFSET, (uint8_t *)&_seed, 2);
        EasyCon_write_end(0);
    } else {
        _seed = (uint16_t)EasyCon_read_byte(SEED_OFFSET + 1) << 8 | EasyCon_read_byte(SEED_OFFSET);
        _seed += 1;
        srand(_seed);
        EasyCon_write_start(1);
        EasyCon_write_data(SEED_OFFSET, (uint8_t *)&_seed, 2);
        EasyCon_write_end(1);
    }
    memset(mem, 0, sizeof(mem));

    for (int i = 0; i < 16; i++) {
        int x = (Min(i, 8)) << 5;
        int y = (Max(i, 8) - 8) << 5;
        x = Min(x, 255);
        y = Min(y, 255);
        DX(i) = x;
        DY(i) = y;
    }
    for (int i = 16; i < 32; i++) {
        int x = (24 - Min(i, 24)) << 5;
        int y = (32 - Max(i, 24)) << 5;
        x = Min(x, 255);
        y = Min(y, 255);
        DX(i) = x;
        DY(i) = y;
    }
    _report_echo = ECHO_TIMES;
    _ledflag = EasyCon_read_byte(LED_SETTING);
    auto_run = (EasyCon_read_byte(1) >> 7) == 0;
}

void EasyCon_script_tick(void) {
    if (wait_ms != 0 && (_report_echo == 0 || wait_ms > 1))
        wait_ms--;
}

void EasyCon_tick(void) {
    timer_ms++;
    if (echo_ms != 0)
        echo_ms--;
    EasyCon_script_tick();
}

void EasyCon_report_send_callback(void) {
    if (!_script_running || _report_echo > 0 || wait_ms < 2) {
        _report_echo = Max(0, _report_echo - 1);
    }
    echo_ms = ECHO_INTERVAL;
}

void EasyCon_script_auto_start(void) {
    if (auto_run)
        EasyCon_script_start();
}

void EasyCon_script_start(void) {
    script_addr = 2;
    uint16_t eof = EasyCon_read_byte(0) | (EasyCon_read_byte(1) << 8);
    if (eof == 0xFFFF)
        eof = 0;
    script_eof = (eof & 0x7FFF);
    wait_ms = 0;
    echo_ms = 0;
    timer_ms = 0;
    tail_wait = 0;
    memset(mem + VARSPACE_OFFSET, 0, sizeof(mem) - VARSPACE_OFFSET);
    _script_running = 1;
    _seed = (uint16_t)EasyCon_read_byte(SEED_OFFSET + 1) << 8 | EasyCon_read_byte(SEED_OFFSET);
}

void EasyCon_script_stop(void) {
    _script_running = 0;
    timer_elapsed = timer_ms;
    reset_hid_report();
    _report_echo = ECHO_TIMES;
}

void EasyCon_script_task(void) {
    while (1) {
        if (!_script_running)
            return;
        if (wait_ms > 0)
            return;
        for (int i = 0; i <= KEYCODE_MAX; i++) {
            if (KEY(i) != 0) {
                KEY(i)--;
                if (KEY(i) == 0) {
                    if (i == 32) {
                        set_left_stick(STICK_CENTER, STICK_CENTER);
                        _report_echo = ECHO_TIMES;
                    } else if (i == 33) {
                        set_right_stick(STICK_CENTER, STICK_CENTER);
                        _report_echo = ECHO_TIMES;
                    } else if ((i & 0x10) == 0) {
                        release_buttons(_BV(i));
                        _report_echo = ECHO_TIMES;
                    } else {
                        set_HAT_switch(HAT_CENTERED);
                        _report_echo = ECHO_TIMES;
                    }
                }
            }
        }
        if (tail_wait != 0) {
            SETWAIT(tail_wait);
            tail_wait = 0;
            return;
        }
        if (script_addr >= script_eof) {
            EasyCon_script_stop();
            return;
        }
        _addr = script_addr;
        _ins0 = EasyCon_read_byte(script_addr);
        script_addr++;
        _ins1 = EasyCon_read_byte(script_addr);
        script_addr++;
        _ins_word = (uint16_t)_ins0 | ((uint16_t)_ins1 << 8);
        _ins2 = EasyCon_read_byte(script_addr);
        script_addr++;
        _ins3 = EasyCon_read_byte(script_addr);
        script_addr++;
        _ins_dword = (uint32_t)_ins0 | ((uint32_t)_ins1 << 8) | ((uint32_t)_ins2 << 16) | ((uint32_t)_ins3 << 24);
        int32_t n;
        int16_t reg;
        if (_ins0 & 0x80) {
            if ((_ins0 & 0x40) == 0) {
                _keycode = (_ins0 >> 1) & 0x1F;
                if ((_keycode & 0x10) == 0) {
                    press_buttons(_BV(_keycode));
                    _report_echo = ECHO_TIMES;
                } else {
                    set_HAT_switch(_keycode & 0xF);
                    _report_echo = ECHO_TIMES;
                }
                if (E_SET) {
                    SETWAIT(REG(_e_val));
                    RESETAFTER(_keycode, 1);
                } else if ((_ins0 & 0x01) == 0) {
                    n = _ins1;
                    n *= 10;
                    SETWAIT(n);
                    RESETAFTER(_keycode, 1);
                } else if ((_ins1 & 0x80) == 0) {
                    tail_wait = _ins1 & 0x7F;
                    tail_wait *= 50;
                    SETWAIT(50);
                    RESETAFTER(_keycode, 1);
                } else {
                    n = _ins1 & 0x7F;
                    RESETAFTER(_keycode, n);
                }
            } else {
                _lr = (_ins0 >> 5) & 1;
                _keycode = 32 | _lr;
                _direction = _ins0 & 0x1F;
                if (_lr) {
                    set_right_stick(DX(_direction), DY(_direction));
                    _report_echo = ECHO_TIMES;
                } else {
                    set_left_stick(DX(_direction), DY(_direction));
                    _report_echo = ECHO_TIMES;
                }
                if (E_SET) {
                    SETWAIT(REG(_e_val));
                    RESETAFTER(_keycode, 1);
                } else if ((_ins1 & 0x80) == 0) {
                    n = _ins1 & 0x7F;
                    n *= 50;
                    SETWAIT(n);
                    RESETAFTER(_keycode, 1);
                } else {
                    n = _ins1 & 0x7F;
                    RESETAFTER(_keycode, n);
                }
            }
        } else {
            switch ((_ins0 >> 3) & 0x0F) {
            case 0x00:
                if ((_ins0 & 0x04) != 0) {
                    reg = _ins & ((1 << 9) - 1);
                    if ((_ins0 & 0x02) == 0) {
                        EasyCon_serial_send(reg);
                        EasyCon_serial_send(reg >> 8);
                    } else {
                        EasyCon_serial_send(mem[reg]);
                        EasyCon_serial_send(mem[reg + 1]);
                    }
                }
                break;
            case 0x01:
                if (E_SET) {
                    n = REG(_e_val);
                } else if ((_ins0 & 0x04) == 0) {
                    n = _ins & ((1 << 10) - 1);
                    n *= 10;
                } else if ((_ins0 & 0x02) == 0) {
                    _ins2 = EasyCon_read_byte(script_addr++);
                    _ins3 = EasyCon_read_byte(script_addr++);
                    n = _insEx & ((1L << 25) - 1);
                    n *= 10;
                } else {
                    n = _ins & ((1 << 9) - 1);
                }
                SETWAIT(n);
                break;
            case 0x02:
                if (_forstackindex == 0 || FOR_ADDR(_forstackindex - 1) != _addr) {
                    _forstackindex++;
                    FOR_I(_forstackindex - 1) = 0;
                    FOR_ADDR(_forstackindex - 1) = _addr;
                    FOR_NEXT(_forstackindex - 1) = _ins & ((1 << 11) - 1);
                    if (E_SET) {
                        _ri0 = REG(_e_val) & 0xF;
                        if (_ri0 != 0) {
                            FOR_I(_forstackindex - 1) = _ri0 | 0x80000000;
                        }
                        _ri1 = (REG(_e_val) >> 4) & 0xF;
                        if (_ri1 != 0) {
                            FOR_C(_forstackindex - 1) = REG(_ri1);
                            E(2);
                            JUMP(FOR_NEXT(_forstackindex - 1));
                            break;
                        }
                    }
                    E(0);
                    JUMP(FOR_NEXT(_forstackindex - 1));
                    break;
                }
                break;
            case 0x03:
                if (_ins0 & 0x04) {
                    _ins2 = EasyCon_read_byte(script_addr++);
                    _ins3 = EasyCon_read_byte(script_addr++);
                }
                if (E_SET) {
                    if (_e_val == 1) {
                        _forstackindex--;
                        break;
                    } else if (_e_val == 0) {
                        if ((_ins0 & 0x04) == 0) {
                            FOR_C(_forstackindex - 1) = _ins & ((1 << 10) - 1);
                            if (FOR_C(_forstackindex - 1) == 0) {
                                FOR_C(_forstackindex - 1) = 0x80000000;
                            }
                        } else {
                            FOR_C(_forstackindex - 1) = _insEx & ((1L << 26) - 1);
                        }
                    }
                } else {
                    if (FOR_I(_forstackindex - 1) & 0x80000000) {
                        REG(FOR_I(_forstackindex - 1) & 0xF) += 1;
                    } else {
                        FOR_I(_forstackindex - 1) += 1;
                    }
                }
                if (FOR_I(_forstackindex - 1) & 0x80000000)
                    n = REG(FOR_I(_forstackindex - 1) & 0xF);
                else
                    n = FOR_I(_forstackindex - 1);
                if (FOR_C(_forstackindex - 1) != 0x80000000 && n >= FOR_C(_forstackindex - 1)) {
                    _forstackindex--;
                } else {
                    JUMP(FOR_ADDR(_forstackindex - 1));
                }
                break;
            case 0x04:
                if (_ins0 & 0x04) {
                    _ri0 = (_ins1 >> 3) & 0x07;
                    _ri1 = _ins1 & 0x07;
                    switch (_ins0 & 0x03) {
                    case 0x00: _v = REG(_ri0) == REG(_ri1); break;
                    case 0x01: _v = REG(_ri0) != REG(_ri1); break;
                    case 0x02: _v = REG(_ri0) < REG(_ri1); break;
                    case 0x03: _v = REG(_ri0) <= REG(_ri1); break;
                    }
                    _v = (bool)_v;
                    _flag = (bool)_flag;
                    switch (_ins1 >> 6) {
                    case 0x00: _flag = _v; break;
                    case 0x01: _flag &= _v; break;
                    case 0x02: _flag |= _v; break;
                    case 0x03: _flag ^= _v; break;
                    }
                } else {
                    switch (_ins1 >> 5) {
                    case 0x00:
                        if ((_ins1 & 0x10) && !_flag) break;
                        _v = _ins1 & 0x0F;
                        _forstackindex -= _v;
                        E(1);
                        JUMP(FOR_NEXT(_forstackindex - 1));
                        break;
                    case 0x01:
                        if ((_ins1 & 0x10) && !_flag) break;
                        _v = _ins1 & 0x0F;
                        _forstackindex -= _v;
                        JUMP(FOR_NEXT(_forstackindex - 1));
                        break;
                    case 0x07:
                        if ((_ins1 & 0x10) && !_flag) break;
                        if (_callstackindex) {
                            JUMP(CALLSTACK(_callstackindex - 1));
                            _callstackindex--;
                        } else {
                            EasyCon_script_stop();
                        }
                        break;
                    }
                }
                break;
            case 0x05:
                if ((_ins0 & 0x04) == 0) {
                    _ri0 = (_ins >> 7) & 0x07;
                    if (_ri0 == 0) {
                        if ((_ins1 & (1 << 6)) == 0) {
                            _ins2 = EasyCon_read_byte(script_addr++);
                            _ins3 = EasyCon_read_byte(script_addr++);
                            _v = (_ins >> 3) & 0x07;
                            _ri0 = _ins & 0x07;
                            reg = _insEx;
                            binaryop(_v, _ri0, reg);
                        }
                    } else {
                        reg = _ins1 & 0x7F;
                        reg <<= 9;
                        reg >>= 9;
                        REG(_ri0) = reg;
                    }
                } else if ((_ins0 & 0x06) == 0x04) {
                    _v = (_ins >> 6) & 0x07;
                    _ri0 = (_ins >> 3) & 0x07;
                    _ri1 = _ins & 0x07;
                    binaryop(_v, _ri0, REG(_ri1));
                } else if ((_ins0 & 0x07) == 0x06) {
                    _ri0 = (_ins1 >> 4) & 0x07;
                    _v = _ins1 & 0x0F;
                    if (_ri0 == 0) break;
                    if ((_ins1 & 0x80) == 0) {
                        REG(_ri0) <<= _v;
                    } else {
                        REG(_ri0) >>= _v;
                    }
                } else {
                    _ri0 = _ins1 & 0x07;
                    switch ((_ins1 >> 3) & 0x0F) {
                    case 0x02: if (_ri0 != 0) REG(_ri0) = -REG(_ri0); break;
                    case 0x03: if (_ri0 != 0) REG(_ri0) = ~REG(_ri0); break;
                    case 0x04: _stackindex++; STACK(_stackindex - 1) = REG(_ri0); break;
                    case 0x05: if (_ri0 != 0) { REG(_ri0) = STACK(_stackindex - 1); _stackindex--; } break;
                    case 0x07: E(_ri0); break;
                    case 0x08: if (_ri0 != 0) REG(_ri0) = (bool)REG(_ri0); break;
                    case 0x09:
                        if (_ri0 != 0) {
                            if (!_seed) {
                                _seed = timer_ms;
                                EasyCon_write_start(1);
                                EasyCon_write_data(SEED_OFFSET, (uint8_t *)&_seed, 2);
                                EasyCon_write_end(1);
                                srand(_seed);
                            }
                            REG(_ri0) = rand() % REG(_ri0);
                        }
                        break;
                    }
                }
                break;
            case 0x06:
                reg = _ins & ((1 << 9) - 1);
                reg = reg << 7 >> 6;
                switch ((_ins0 >> 1) & 0x3) {
                case 0x00: JUMPNEAR(reg); break;
                case 0x01: if (_flag) JUMPNEAR(reg); break;
                case 0x02: if (!_flag) JUMPNEAR(reg); break;
                case 0x03:
                    _callstackindex++;
                    CALLSTACK(_callstackindex - 1) = (int16_t)script_addr;
                    JUMPNEAR(reg);
                    break;
                }
                break;
            }
        }
    }
}

static void binaryop(uint8_t op, uint8_t reg, int16_t value) {
    if (reg == 0) return;
    switch (op) {
    case 0x00: REG(reg) = value; break;
    case 0x01: REG(reg) += value; break;
    case 0x02: REG(reg) *= value; break;
    case 0x03: REG(reg) /= value; break;
    case 0x04: REG(reg) %= value; break;
    case 0x05: REG(reg) &= value; break;
    case 0x06: REG(reg) |= value; break;
    case 0x07: REG(reg) ^= value; break;
    }
}

void EasyCon_serial_task(int16_t byte) {
    if (byte < 0) return;
    if (flash_index < flash_count) {
        serial_buffer[flash_index] = (uint8_t)byte;
        flash_index++;
        if (flash_index == flash_count) {
            EasyCon_write_start(2);
            EasyCon_write_data(flash_addr, serial_buffer, flash_count);
            EasyCon_serial_send(REPLY_FLASHEND);
        }
    } else {
        serial_buffer[serial_buffer_length++] = (uint8_t)byte;
        if ((byte & 0x80) != 0) {
            if (serial_buffer_length == 1 && !serial_command_ready && byte == CMD_READY) {
                serial_command_ready = true;
            } else if (serial_buffer_length == 8) {
                if (_script_running) {
                    EasyCon_serial_send(REPLY_BUSY);
                } else {
                    set_buttons((serial_buffer[0] << 9) | (serial_buffer[1] << 2) | (serial_buffer[2] >> 5));
                    set_HAT_switch((uint8_t)((serial_buffer[2] << 3) | (serial_buffer[3] >> 4)));
                    set_left_stick((uint8_t)((serial_buffer[3] << 4) | (serial_buffer[4] >> 3)),
                                   (uint8_t)((serial_buffer[4] << 5) | (serial_buffer[5] >> 2)));
                    set_right_stick((uint8_t)((serial_buffer[5] << 6) | (serial_buffer[6] >> 1)),
                                    (uint8_t)((serial_buffer[6] << 7) | (serial_buffer[7] & 0x7f)));
                    _report_echo = ECHO_TIMES;
                    if (hid_device_is_ready()) {
                        hid_device_send();
                        EasyCon_serial_send(REPLY_ACK);
                    } else {
                        EasyCon_serial_send(REPLY_ERROR);
                    }
                }
                serial_command_ready = false;
            } else if (serial_command_ready) {
                // 保持serial_command_ready为true，以便连续接收命令
                switch (byte) {
                case CMD_DEBUG: {
                    uint32_t n;
                    n = FOR_I(_forstackindex - 1);
                    for (int i = 0; i < 4; i++) { EasyCon_serial_send(n); n >>= 8; }
                    n = timer_elapsed;
                    for (int i = 0; i < 4; i++) { EasyCon_serial_send(n); n >>= 8; }
                    n = script_addr;
                    for (int i = 0; i < 2; i++) { EasyCon_serial_send(n); n >>= 8; }
                    uint32_t sc = hid_device_get_send_count();
                    uint32_t fc = hid_device_get_fail_count();
                    EasyCon_serial_send(sc & 0xFF);
                    EasyCon_serial_send((sc >> 8) & 0xFF);
                    EasyCon_serial_send(fc & 0xFF);
                    EasyCon_serial_send((fc >> 8) & 0xFF);
                    break;
                }
                case CMD_VERSION:
                    EasyCon_serial_send(VERSION);
                    break;
                case CMD_LED:
                    _ledflag ^= 0x8;
                    EasyCon_write_start(1);
                    EasyCon_write_data(LED_SETTING, (uint8_t *)&_ledflag, 1);
                    EasyCon_write_end(1);
                    EasyCon_serial_send(_ledflag);
                    break;
                case CMD_READY:
                    serial_command_ready = true;
                    break;
                case CMD_HELLO:
                    EasyCon_serial_send(REPLY_HELLO);
                    break;
                case CMD_FLASH:
                    if (serial_buffer_length != 5) {
                        EasyCon_serial_send(REPLY_ERROR);
                        break;
                    }
                    EasyCon_script_stop();
                    flash_addr = (uint16_t)(serial_buffer[0] | (serial_buffer[1] << 7));
                    flash_count = (serial_buffer[2] | (serial_buffer[3] << 7));
                    flash_index = 0;
                    EasyCon_serial_send(REPLY_FLASHSTART);
                    break;
                case CMD_SCRIPTSTART:
                    EasyCon_script_start();
                    EasyCon_serial_send(REPLY_SCRIPTACK);
                    break;
                case CMD_SCRIPTSTOP:
                    EasyCon_script_stop();
                    EasyCon_serial_send(REPLY_SCRIPTACK);
                    break;
                default:
                    EasyCon_serial_send(REPLY_ERROR);
                    break;
                }
                if (byte != CMD_FLASH && byte != CMD_HELLO && byte != CMD_VERSION && byte != CMD_DEBUG && byte != CMD_READY && byte != CMD_SCRIPTSTART && byte != CMD_SCRIPTSTOP) {
                    EasyCon_write_end(0);
                }
            } else {
                EasyCon_serial_send(serial_buffer_length);
            }
            serial_buffer_length = 0;
        }
        if (serial_buffer_length >= SERIAL_BUFFER_SIZE)
            serial_buffer_length = 0;
    }
}

bool EasyCon_need_send_report(void) {
    return (echo_ms == 0);
}

void set_buttons(uint16_t buttons) {
    switch_hid_report_t r;
    hid_device_get_report(&r);
    r.Button = buttons;
    hid_device_set_report(&r);
}

void press_buttons(uint16_t buttons) {
    switch_hid_report_t r;
    hid_device_get_report(&r);
    r.Button |= buttons;
    hid_device_set_report(&r);
}

void release_buttons(uint16_t buttons) {
    switch_hid_report_t r;
    hid_device_get_report(&r);
    r.Button &= ~buttons;
    hid_device_set_report(&r);
}

void set_HAT_switch(uint8_t hat) {
    switch_hid_report_t r;
    hid_device_get_report(&r);
    r.HAT = hat;
    hid_device_set_report(&r);
}

void set_left_stick(uint8_t lx, uint8_t ly) {
    switch_hid_report_t r;
    hid_device_get_report(&r);
    r.LX = lx;
    r.LY = ly;
    hid_device_set_report(&r);
}

void set_right_stick(uint8_t rx, uint8_t ry) {
    switch_hid_report_t r;
    hid_device_get_report(&r);
    r.RX = rx;
    r.RY = ry;
    hid_device_set_report(&r);
}

void reset_hid_report(void) {
    switch_hid_report_t r = {
        .LX = STICK_CENTER, .LY = STICK_CENTER,
        .RX = STICK_CENTER, .RY = STICK_CENTER,
        .HAT = HAT_CENTERED, .Button = 0, .VendorSpec = 0
    };
    hid_device_set_report(&r);
}
