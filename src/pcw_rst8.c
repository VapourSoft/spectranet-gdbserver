#include <stdint.h>
#include "state.h"
#include "pcw_rst8.h"
#include "server.h"
#include "pcw_dart.h"
#include "utils.h"
#include "z80_decode.h"

extern struct gdbserver_state_t gdbserver_state;

// Assembly writes current SP to _rst8_sp_copy before C is invoked
extern uint16_t rst8_sp_copy; // storage in pcw_rst8.asm

/*
 Stack (lowest address first) when rst8_c_trap runs:
   0: IY
   1: IX
   2: HL
   3: DE
   4: BC
   5: AF
   6: ret_addr (address AFTER RST 08 byte)
*/
extern void printS(const char* str) __z88dk_fastcall ;

uint16_t trap_restore_idx;


//msg must be $ terminated!!!
void log(const char *msg, uint16_t addr)
{
    char hexbuf[5];
    printS(msg);
    char_to_hex(hexbuf, (addr >> 8) & 0xFF);
    char_to_hex(hexbuf + 2, addr & 0xFF);
    hexbuf[4] = '$';
    printS(hexbuf);      
    printS("\r\n$");
}


void rst8_c_trap(void)
{
  // Print debug message to the screen using BDOS call
  printS("[RST08!     ]\r\n$");

//  char hexbuf[5];
  // Stack layout after pushes (each push 2 bytes):
  // push af, bc, de, hl, ix, iy  (last pushed at lowest address)
  // Memory ascending: IY, IX, HL, DE, BC, AF, ret_addr
  // Total pushed: 12 bytes registers + 2 ret = 14?  Actually ret was on stack before pushes.
  // After pushes SP_new = original_SP - 12. rst8_sp_copy holds original SP (before pushes).
  uint16_t *base = (uint16_t*)(rst8_sp_copy - 12);
  uint16_t ret_addr = base[6];

  // User SP before our pushes = (SP_new + 12) = rst8_sp_copy
  uint16_t user_sp = rst8_sp_copy;

  int trapped_breakpoint = -1;
  int enter_gdb_loop = 0;

  gdbserver_state.trap_flags &= (uint8_t)~TRAP_FLAG_BREAK_HIT;

  // Handle temporary (single-step) breakpoint
  if (gdbserver_state.temporary_breakpoint.address &&
      gdbserver_state.temporary_breakpoint.address == (ret_addr - 1)) {
    
      // We've hit the temp breakpoint for single-step
      gdbserver_state.trap_flags = (gdbserver_state.trap_flags | TRAP_FLAG_BREAK_HIT);      

      // Restore original instruction
      *(uint8_t*)(gdbserver_state.temporary_breakpoint.address) = gdbserver_state.temporary_breakpoint.original_instruction;

      if (gdbserver_state.trap_flags & TRAP_FLAG_RESTORE_RST08H)
      {
          // We were stepping to restore the original BP at the previous address
          gdbserver_state.breakpoints[trap_restore_idx].original_instruction = *(uint8_t*)(gdbserver_state.breakpoints[trap_restore_idx].address);                    
          *(uint8_t*)(  gdbserver_state.breakpoints[trap_restore_idx].address) = 0xCF; // RST 08h
          gdbserver_state.trap_flags &= (uint8_t)~TRAP_FLAG_RESTORE_RST08H;
          log("[REST BP   *] @ $", gdbserver_state.breakpoints[trap_restore_idx].address );          
      }
//      else
      //{
      if (gdbserver_state.trap_flags & TRAP_FLAG_STEP_INSTRUCTION)
      {
          // Flag that we hit the temp breakpoint
          enter_gdb_loop = 1;
          gdbserver_state.trap_flags &= (uint8_t)~TRAP_FLAG_STEP_INSTRUCTION;
          log("[HIT      SS] @ $", gdbserver_state.temporary_breakpoint.address );          
      }
      // Clear temp breakpoint state
      gdbserver_state.temporary_breakpoint.address = 0;
  } 
  else 
  {
    enter_gdb_loop = 1; //it must be either a breakpoint or hard coded RST 08 trap
  }

  if (enter_gdb_loop)
  {
      for (int i = 0; i < MAX_BREAKPOINTS_COUNT; i++) {

        if (gdbserver_state.breakpoints[i].address) {
            // Restore original instructions in case of dissasembly
            *(uint8_t*)(gdbserver_state.breakpoints[i].address) = gdbserver_state.breakpoints[i].original_instruction;
            log("[REST INST  ] @ $", gdbserver_state.breakpoints[i].address );          

            //Check if we hit the BP
            if (gdbserver_state.breakpoints[i].address == (ret_addr - 1)) {
              trapped_breakpoint = i;
              gdbserver_state.trap_flags = (gdbserver_state.trap_flags | TRAP_FLAG_BREAK_HIT);
              log("[HIT      BP] @ $", gdbserver_state.breakpoints[i].address );          
              //break;
            }
        }
      }
  }



  gdbserver_state.registers[REGISTERS_PC] = (uint16_t)(ret_addr - 1); // address of breakpoint instr (RST 08)
  gdbserver_state.registers[REGISTERS_SP] = user_sp;                  // SP at trap (points to ret_addr)

  //read registers from stack order they are saved on stack is important !! see pcw_rst8.asm
  gdbserver_state.registers[REGISTERS_IY] = base[0];
  gdbserver_state.registers[REGISTERS_IX] = base[1];  
  gdbserver_state.registers[REGISTERS_HL] = base[2];
  gdbserver_state.registers[REGISTERS_DE] = base[3];
  gdbserver_state.registers[REGISTERS_BC] = base[4];
  gdbserver_state.registers[REGISTERS_AF] = base[5];

  //Only enter GDB server loop if we hit a breakpoint or step (not for trap RST 08 restore)
  if (enter_gdb_loop )
  {
    printS("[    GDB    ]\r\n$");
    // Always tell GDB we've stopped (breakpoint or step)
    server_write_packet("T05thread:p01.01;");
    // Enter GDB server main loop (wait for GDB connection/commands)
    while (server_read_data()) {
      // loop until GDB says continue/step
    }
  }

  // Write back modified registers to stack so they are restored by the pops/ret in rst8_entry
  base[0] = gdbserver_state.registers[REGISTERS_IY];
  base[1] = gdbserver_state.registers[REGISTERS_IX];
  base[2] = gdbserver_state.registers[REGISTERS_HL];
  base[3] = gdbserver_state.registers[REGISTERS_DE];
  base[4] = gdbserver_state.registers[REGISTERS_BC];
  base[5] = gdbserver_state.registers[REGISTERS_AF];


  //Need to figure out how to restore the BP on resume ... we need the original instruction back (done above) but then when we resume how can we put the BP back ?
  //maybe we set a special temp breakpoint for next instructiion, with flag to put BP back and then contiue automatically ?
  // TODO: Shouldnt we restore the RST 08 if the breakpoint is still there, regardless of PC?

  // Check if we are on a breakpoint in case we need to restore RST 08
  if (enter_gdb_loop){  
    for (int i = 0; i < MAX_BREAKPOINTS_COUNT; i++) {

        if (gdbserver_state.breakpoints[i].address) {
          if (gdbserver_state.breakpoints[i].address ==  gdbserver_state.registers[REGISTERS_PC]) {
            // Defer to restore BP as we cant do now (we will continue from here) 
            trap_restore_idx = i;
            gdbserver_state.trap_flags |= TRAP_FLAG_RESTORE_RST08H;
          }
          else
          {
            log("[SET BP     ] @ $", gdbserver_state.breakpoints[i].address );                    
            
            //Record the existing instruction          
            gdbserver_state.breakpoints[i].original_instruction = *(uint8_t*)(gdbserver_state.breakpoints[i].address);

            if (gdbserver_state.breakpoints[i].original_instruction == 0xCF) //Only set RST08 if not already there
              log("[ERR ORG 0CF] @ $", gdbserver_state.breakpoints[i].address );                    

            *(uint8_t*)(gdbserver_state.breakpoints[i].address) = 0xCF; //SET RST08            
          }
      }
    }
  }

  // If step requested set temp breakpoint at next instruction
  if ((gdbserver_state.trap_flags & TRAP_FLAG_STEP_INSTRUCTION || gdbserver_state.trap_flags & TRAP_FLAG_RESTORE_RST08H ) /*&& !temp_breakpoint_hit */) {

    // Use robust Z80 instruction decoder to find next instruction address
    uint16_t next_addr = calculateStep();
    gdbserver_state.temporary_breakpoint.address = next_addr;
    gdbserver_state.temporary_breakpoint.original_instruction = *(uint8_t*)next_addr;
    *(uint8_t*)next_addr = 0xCF; // RST 08h
    
    if (gdbserver_state.trap_flags & TRAP_FLAG_STEP_INSTRUCTION)
      log("                    [SET STEP TB] @ $", next_addr );          

    if (gdbserver_state.trap_flags & TRAP_FLAG_RESTORE_RST08H)
      log("                    [SET REST TB] @ $" , next_addr );          
  }


  // Determine how to resume: use trapped_breakpoint or temp_breakpoint state
  uint16_t resume_addr = 0;
  if (gdbserver_state.registers[REGISTERS_PC] != (uint16_t)(ret_addr - 1)) {
    // User changed PC in GDB, resume at new PC
    resume_addr = gdbserver_state.registers[REGISTERS_PC];
    log("                                        [CONT NEW PC] @ $", resume_addr );          
  } else if (gdbserver_state.trap_flags & TRAP_FLAG_BREAK_HIT ) {
    // Resume at the breakpoint address (original instruction restored)
    resume_addr = (uint16_t)(ret_addr - 1);
    log("                                        [CONT       ] @ $", resume_addr );              
  } else {
    // Explicit RST 08 in user code: resume after RST 08
    resume_addr = ret_addr;
    log("                                        [CONT > RST8] @ $", resume_addr );                  
  }
  base[6] = resume_addr;

}
