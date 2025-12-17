#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "chip8.h"

static uint8_t chip8_fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

chip8 *chip8_init() {
  chip8 *vm = (chip8 *)calloc(1, sizeof(chip8));
  if (!vm) {
    fprintf(stderr, "Not enough memory available, exiting...\n");
    exit(1);
  }

  for (int i = 0; i < 80; i++) {
    vm->memory[i] = chip8_fontset[i];
  }

  vm->running = 1;
  vm->PC = 0x200;
  vm->opcode = 0;
  vm->I = 0;
  vm->draw = 0;
  vm->SP = 0;
  memset(vm->V, 0, sizeof(uint8_t));

  srand(time(NULL));

  return vm;
}

void chip8_destroy(chip8 *vm) {
  free(vm);
  fflush(stdout);
}

void opcode(chip8 *vm) {

  vm->opcode = vm->memory[vm->PC] << 8 | vm->memory[vm->PC + 1];
  int jumped = 0;

  switch (vm->opcode & 0xF000) {

  case 0x0000:
    switch (vm->opcode & 0x00FF) {

    /* 00E0 - CLS
    Clear the display.*/
    case 0x00E0: {
      memset(vm->memory + 0x200, 0, sizeof(uint8_t));
      for (int i = 0; i < 64; i++) {
        memset(vm->gfx[i], 0, sizeof(unsigned char));
      }
    } break;

    /* 00EE - RET
    Return from a subroutine.

    The interpreter sets the program counter to the address at the top of the
    stack, then subtracts 1 from the stack pointer.*/
    case 0x00EE: {
      vm->PC = vm->SP & 0x00FF;
      vm->SP--;
    } break;

    default:
      fprintf(stderr, "Invalid opcode: 0x%.4X\n", vm->opcode);
      break;
    }
    break;

  /* 1nnn - JP addr
  Jump to location nnn.

  The interpreter sets the program counter to nnn.*/
  case 0x1000: {
    jumped = 1;
    vm->PC = vm->opcode & 0x0FFF;
  } break;

  /* 2nnn - CALL addr
  Call subroutine at nnn.

  The interpreter increments the stack pointer, then puts the current PC on the
  top of the stack. The PC is then set to nnn.*/
  case 0x2000: {
    vm->SP++;
    vm->stack[vm->SP] = vm->PC;
    vm->PC = vm->opcode & 0x0FFF;
  } break;

  /* 3xkk - SE Vx, byte
  Skip next instruction if Vx = kk.

  The interpreter compares register Vx to kk, and if they are equal,
  increments the program counter by 2.*/
  case 0x3000: {
    uint8_t Vx = (vm->opcode & 0x0F00) >> 8, kk = vm->opcode & 0x00FF;
    if (Vx == 0x0F) {
      fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
    } else if (vm->V[Vx] == kk) {
      vm->PC += 2;
    }
  } break;

  /* 4xkk - SNE Vx, byte
  Skip next instruction if Vx != kk.

  The interpreter compares register Vx to kk, and if they are not equal,
  increments the program counter by 2.*/
  case 0x4000: {
    uint8_t Vx = (vm->opcode & 0x0F00) >> 8, kk = vm->opcode & 0x00FF;
    if (Vx == 0x0F) {
      fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
    } else if (vm->V[Vx] != kk) {
      vm->PC += 2;
    }
  } break;

  /* 5xy0 - SE Vx, Vy
  Skip next instruction if Vx = Vy.

  The interpreter compares register Vx to register Vy, and if they are equal,
  increments the program counter by 2.*/
  case 0x5000: {
    uint8_t Vx = (vm->opcode & 0x0F00) >> 8;
    uint8_t Vy = (vm->opcode & 0x00F0) >> 4;
    if (Vx == 0x0F || Vy == 0x0F) {
      fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
    } else if (vm->V[Vx] == vm->V[Vy]) {
      vm->PC += 2;
    }
  } break;

  /* 6xkk - LD Vx, byte
  Set Vx = kk.

  The interpreter puts the value kk into register Vx.*/
  case 0x6000: {
    uint8_t Vx = (vm->opcode & 0x0F00) >> 8, kk = vm->opcode & 0x00FF;
    if (Vx == 0x0F) {
      fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
    } else {
      vm->V[Vx] = kk;
    }
  } break;

  /* 7xkk - ADD Vx, byte
  Set Vx = Vx + kk.

  Adds the value kk to the value of register Vx, then stores the result in Vx.*/
  case 0x7000: {
    uint8_t Vx = (vm->opcode & 0x0F00) >> 8, kk = vm->opcode & 0x00FF;
    if (Vx == 0x0F) {
      fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
    } else {
      vm->V[Vx] = (vm->V[Vx] + kk) & 0x00FF;
    }
  } break;

  case 0x8000:
    switch (vm->opcode & 0x000F) {
    /* 8xy0 - LD Vx, Vy
    Set Vx = Vy.

    Stores the value of register Vy in register Vx.*/
    case 0x0000: {
      uint8_t Vx = (vm->opcode & 0x0F00) >> 8, Vy = (vm->opcode & 0x00F0) >> 4;
      if (Vx == 0x0E || Vy == 0x0E) {
        fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
      } else {
        vm->V[Vx] = vm->V[Vy];
      }
    } break;

    /* 8xy1 - OR Vx, Vy
    Set Vx = Vx OR Vy.

    Performs a bitwise OR on the values of Vx and Vy, then stores the result in
    Vx. A bitwise OR compares the corresponding bits from two values, and if
    either bit is 1, then the same bit in the result is also 1. Otherwise, it is
    0.*/
    case 0x0001: {
      uint8_t Vx = (vm->opcode & 0x0F00) >> 8, Vy = (vm->opcode & 0x00F0) >> 4;
      if (Vx == 0x0F || Vy == 0x0F) {
        fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
      } else {
        vm->V[Vx] = vm->V[Vx] | vm->V[Vy];
      }
    } break;

    /* 8xy2 - AND Vx, Vy
    Set Vx = Vx AND Vy.

    Performs a bitwise AND on the values of Vx and Vy, then stores the result in
    Vx. A bitwise AND compares the corrseponding bits from two values, and if
    both bits are 1, then the same bit in the result is also 1. Otherwise, it is
    0.*/
    case 0x0002: {
      uint8_t Vx = (vm->opcode & 0x0F00) >> 8, Vy = (vm->opcode & 0x00F0) >> 4;
      if (Vx == 0x0F || Vy == 0x0F) {
        fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
      } else {
        vm->V[Vx] = vm->V[Vx] & vm->V[Vy];
      }
    } break;

    /* 8xy3 - XOR Vx, Vy
    Set Vx = Vx XOR Vy.

    Performs a bitwise exclusive OR on the values of Vx and Vy, then stores the
    result in Vx. An exclusive OR compares the corrseponding bits from two
    values, and if the bits are not both the same, then the corresponding bit in
    the result is set to 1. Otherwise, it is 0. */
    case 0x0003: {
      uint8_t Vx = (vm->opcode & 0x0F00) >> 8, Vy = (vm->opcode & 0x00F0) >> 4;
      if (Vx == 0x0F && Vy == 0x0F) {
        fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
      } else {
        vm->V[Vx] = vm->V[Vx] ^ vm->V[Vy];
      }
    } break;

    /* 8xy4 - ADD Vx, Vy
    Set Vx = Vx + Vy, set VF = carry.

    The values of Vx and Vy are added together.
    If the result is greater than 8 bits (i.e., > 255,) VF is set to 1,
    otherwise 0. Only the lowest 8 bits of the result are kept, and stored in
    Vx.*/
    case 0x0004: {
      uint8_t Vx = (vm->opcode & 0x0F00) >> 8, Vy = (vm->opcode & 0x00F0) >> 4;
      if (Vx == 0x0F && Vy == 0x0F) {
        fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
      } else {
        vm->V[Vx] = vm->V[Vx] + vm->V[Vy];
      }
    } break;

    /* 8xy5 - SUB Vx, Vy
    Set Vx = Vx - Vy, set VF = NOT borrow.

    If Vx > Vy, then VF is set to 1, otherwise 0.
    Then Vy is subtracted from Vx, and the results stored in Vx.*/
    case 0x0005: {
      uint8_t Vx = (vm->opcode & 0x0F00) >> 8, Vy = (vm->opcode & 0x00F0) >> 4;
      if (Vx == 0x0F && Vy == 0x0F) {
        fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
      } else {
        vm->V[0xF] = vm->V[Vx] > vm->V[Vy] ? 1 : 0;
        vm->V[Vx] = vm->V[Vx] - vm->V[Vy];
      }
    } break;

    /* 8xy6 - SHR Vx {, Vy}
    Set Vx = Vx SHR 1.

    If the least-significant bit of Vx is 1, then VF is set to 1, otherwise 0.
    Then Vx is divided by 2.*/
    case 0x0006: {
      uint8_t Vx = (vm->opcode & 0x0F00) >> 8;
      if (Vx == 0x0F) {
        fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
      } else {
        vm->V[0xF] = vm->V[Vx] & 0x01;
        vm->V[Vx] >>= 1;
      }
    } break;

    /* 8xy7 - SUBN Vx, Vy
    Set Vx = Vy - Vx, set VF = NOT borrow.

    If Vy > Vx, then VF is set to 1, otherwise 0.
    Then Vx is subtracted from Vy, and the results stored in Vx.*/
    case 0x0007: {
      uint8_t Vx = (vm->opcode & 0x0F00) >> 8, Vy = (vm->opcode & 0x00F0) >> 4;
      if (Vx == 0x0F && Vy == 0x0F) {
        fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
      } else {
        vm->V[0xF] = vm->V[Vy] > vm->V[Vx] ? 1 : 0;
        vm->V[Vx] = vm->V[Vy] - vm->V[Vx];
      }
    } break;

    /* 8xyE - SHL Vx {, Vy}
    Set Vx = Vx SHL 1.

    If the most-significant bit of Vx is 1, then VF is set to 1, otherwise to 0.
    Then Vx is multiplied by 2.*/
    case 0x000E: {
      uint8_t Vx = (vm->opcode & 0x0F00) >> 8;
      if (Vx == 0x0F) {
        fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
      } else {
        vm->V[0xF] = vm->V[Vx] & 0x10;
        vm->V[Vx] <<= 1;
      }
    } break;

    default:
      fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
      break;
    }
    break;

  /* 9xy0 - SNE Vx, Vy
  Skip next instruction if Vx != Vy.

  The values of Vx and Vy are compared, and if they are not equal,
  the program counter is increased by 2.*/
  case 0x9000: {
    uint8_t Vx = (vm->opcode & 0x0F00) >> 8, Vy = (vm->opcode & 0x00F0) >> 4;
    if (Vx == 0x0F && Vy == 0x0F) {
      fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
    } else if (vm->V[Vx] != vm->V[Vy]) {
      vm->PC += 2;
    }
  } break;

  /* Annn - LD I, addr
  Set I = nnn.

  The value of register I is set to nnn.*/
  case 0xA000: {
    vm->I = vm->opcode & 0x0FFF;
  } break;

  /* Bnnn - JP V0, addr
  Jump to location nnn + V0.

  The program counter is set to nnn plus the value of V0.*/
  case 0xB000: {
    jumped = 1;
    vm->PC = (vm->opcode & 0x0FFF) + vm->V[0];
  } break;

  /* Cxkk - RND Vx, byte
  Set Vx = random byte AND kk.

  The interpreter generates a random number from 0 to 255,
  which is then ANDed with the value kk. The results are stored in Vx.
  See instruction 8xy2 for more information on AND.*/
  case 0xC000: {
    uint8_t Vx = (vm->opcode & 0x0F00) >> 8, kk = vm->opcode & 0x00FF;
    if (Vx == 0x0F) {
      fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
    } else {
      vm->V[Vx] = (uint8_t)rand() & kk;
    }
  } break;

  /* Dxyn - DRW Vx, Vy, nibble
  Display n-byte sprite starting at memory location I at (Vx, Vy), set VF =
  collision.

  The interpreter reads n bytes from memory, starting at the address stored in
  I. These bytes are then displayed as sprites on screen at coordinates (Vx,
  Vy). Sprites are XORed onto the existing screen. If this causes any pixels to
  be erased, VF is set to 1, otherwise it is set to 0. If the sprite is
  positioned so part of it is outside the coordinates of the display, it wraps
  around to the opposite side of the screen. See instruction 8xy3 for more
  information on XOR, and section 2.4, Display, for more information on the
  Chip-8 screen and sprites.*/
  case 0xD000: {
    uint8_t Vx = (vm->opcode & 0x0F00) >> 8, Vy = (vm->opcode & 0x00F0) >> 4;
    if (Vx == 0x0F && Vy == 0x0F) {
      fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
    } else {
      uint8_t n = vm->opcode & 0x000F;
      vm->V[0xF] = 0;

      // using the default "i" and "j" led to errors
      // i and j are often used for array index offsetting like ar[base + i]
      // but in our case we want to offset the value (ar[base] + i)
      // = offset the gfx index
      for (int dy = 0; dy < n; dy++) {
        uint8_t sprite_byte = vm->memory[vm->I + dy];

        for (int dx = 0; dx < 8; dx++) {
          // must check left to right
          if (sprite_byte & (0x80 >> dx)) {
            uint8_t x = (vm->V[Vx] + dx) & 63, y = (vm->V[Vy] + dy) & 31;

            if (vm->gfx[y][x]) {
              vm->V[0xF] = 1;
            }

            vm->gfx[y][x] ^= 1;
          }
        }
      }

      vm->draw = 1;
    }
  } break;

  case 0xE000:
    switch (vm->opcode & 0x00FF) {

    /* Ex9E - SKP Vx
    Skip next instruction if key with the value of Vx is pressed.

    Checks the keyboard, and if the key corresponding to the value of Vx is
    currently in the down position, PC is increased by 2.*/
    case 0x009E: {
      uint8_t Vx = vm->V[(vm->opcode & 0x0F00) >> 8];
      if (vm->key[Vx]) {
        vm->PC += 2;
      }
    } break;

    /* ExA1 - SKNP Vx
    Skip next instruction if key with the value of Vx is not pressed.

    Checks the keyboard, and if the key corresponding to the value of Vx is
    currently in the up position, PC is increased by 2.*/
    case 0x00A1: {
      uint8_t Vx = vm->V[(vm->opcode & 0x0F00) >> 8];
      if (!vm->key[Vx]) {
        vm->PC += 2;
      }
    } break;
    }
    break;

  case 0xF000: {
    uint8_t x = vm->opcode & 0x0F;
    if (x == 0xF) {
      switch (vm->opcode & 0x00FF) {
      /* Fx07 - LD Vx, DT
      Set Vx = delay timer value.

      The value of DT is placed into Vx.*/
      case 0x0007: {
        vm->V[x] = vm->DT;
      } break;

      /* Fx0A - LD Vx, K
      Wait for a key press, store the value of the key in Vx.

      All execution stops until a key is pressed, then the value of that key is
      stored in Vx.*/
      case 0x000A: {
        unsigned char key = '\0';
        while (!key) {
          scanf("%c", &key);
        }
        vm->V[x] = key;
      } break;

      /* Fx15 - LD DT, Vx
      Set delay timer = Vx.

      DT is set equal to the value of Vx.*/
      case 0x0015: {
        vm->DT = vm->V[x];
      } break;

      /* Fx18 - LD ST, Vx
      Set sound timer = Vx.

      ST is set equal to the value of Vx.*/
      case 0x0018: {
        vm->ST = vm->V[x];
      } break;

      /* Fx1E - ADD I, Vx
      Set I = I + Vx.

      The values of I and Vx are added, and the results are stored in I.*/
      case 0x001E: {
        vm->I = vm->I + vm->V[x];
      } break;

      /* Fx29 - LD F, Vx
      Set I = location of sprite for digit Vx.

      The value of I is set to the location for the hexadecimal sprite
      corresponding to the value of Vx. See section 2.4 in the Chip-8 technical
      reference, Display, for more information on the Chip-8 hexadecimal font.*/
      case 0x0029: {
        vm->I = vm->V[x] * 0x5;
      } break;

      /* Fx33 - LD B, Vx
      Store BCD (binary coded decimal) representation of Vx in memory locations
      I, I+1, and I+2.

      The interpreter takes the decimal value of Vx, and places the hundreds
      digit in memory at location in I, the tens digit at location I+1, and the
      ones digit at location I+2.*/
      case 0x0033: {
        vm->memory[vm->I] = vm->V[x] / 100;
        vm->memory[vm->I + 1] = (vm->V[x] / 10) % 10;
        vm->memory[vm->I + 2] = vm->V[x] % 100;
      } break;

      /* Fx55 - LD [I], Vx
      Store registers V0 through Vx in memory starting at location I.

      The interpreter copies the values of registers V0 through Vx into memory,
      starting at the address in I.*/
      case 0x0055: {
        for (uint8_t i = 0; i < x + 1; i++) {
          vm->memory[vm->I + i] = vm->V[i];
        }
      } break;

      /* Fx65 - LD Vx, [I]
      Read registers V0 through Vx from memory starting at location I.

      The interpreter reads values from memory starting at location I into
      registers V0 through Vx.*/
      case 0x0065: {
        for (uint8_t i = 0; i < x; i++) {
          vm->V[i] = vm->memory[vm->I + i];
        }
      } break;
      }
      break;
    }
  } break;

  default:
    fprintf(stderr, "Invalid opcode: 0x%X\n", vm->opcode);
    break;
  }
  if (!jumped) {
    vm->PC += 2;
  }
}
