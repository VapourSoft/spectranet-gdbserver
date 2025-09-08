#include <stdint.h>
#include "state.h"
#include "pcw_rst8.h"
#include "server.h"
#include "pcw_dart.h"
#include "utils.h"
#include "z80_decode.h"

#define DEBUG_RST 1
//#define DEBUG_BP 1
#define  DEBUG_LOG 1

extern struct gdbserver_state_t gdbserver_state;

// Assembly writes current SP to _rst8_sp_copy before C is invoked
extern uint16_t rst8_sp_copy; // storage in pcw_rst8.asm
extern uint16_t our_sp_base;  // our stack base (highest address)

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

#ifdef DEBUG_RST
  extern void printS(const char* str) __z88dk_fastcall ;
#else
  #define printS(x)
#endif

#ifdef DEBUG_LOG
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
#else
  #define log(x,y)
#endif


volatile uint8_t enable_serial_interrupt = 0;

uint16_t trap_restore_idx;


void rst8_c_trap(void)
{
  //prevent monitor of serial for interrupts while in GDB server
  enable_serial_interrupt = 0;

  // Print debug message to the screen using BDOS call
  printS("[RST08!     ]\r\n$");



  uint16_t *saved_registers = (uint16_t*)(our_sp_base);          // access registers saved on stack 
  
  // Obraint the return address from the original stack
  uint16_t rst08_return_address = *(uint16_t*)rst8_sp_copy;      // get the RST08 return address (address after the RST 08 instruction)
  uint16_t rst08_address = rst08_return_address - 1;             // address of the RST 08 instruction

  int trapped_breakpoint = -1;
  int enter_gdb_loop = 0;

  gdbserver_state.trap_flags &= (uint8_t)~TRAP_FLAG_BREAK_HIT;

  // Handle temporary (single-step) breakpoint
  if (gdbserver_state.temporary_breakpoint.address &&
      gdbserver_state.temporary_breakpoint.address == (rst08_address)) {
    
      // We've hit the temp breakpoint for single-step
      gdbserver_state.trap_flags = (gdbserver_state.trap_flags | TRAP_FLAG_BREAK_HIT);      

      // Restore original instruction
      *(uint8_t*)(gdbserver_state.temporary_breakpoint.address) = gdbserver_state.temporary_breakpoint.original_instruction;

      if (gdbserver_state.trap_flags & TRAP_FLAG_RESTORE_RST08H)
      {
          // We were stepping to restore the original BP at the previous address
          gdbserver_state.breakpoints[trap_restore_idx].original_instruction = *(uint8_t*)(gdbserver_state.breakpoints[trap_restore_idx].address);                    

          //extra check that the instruction is still the same (ie user didnt change it) - not foolproof but better than nothing
          if (gdbserver_state.breakpoints[trap_restore_idx].original_instruction == *(uint8_t*)(gdbserver_state.breakpoints[trap_restore_idx].address))
          {
            *(uint8_t*)(  gdbserver_state.breakpoints[trap_restore_idx].address) = 0xCF; // RST 08h
            log("[REST BP   *] @ $", gdbserver_state.breakpoints[trap_restore_idx].address );          
          }
          else
          {
            log("[*ERR RESTBP] @ $", gdbserver_state.breakpoints[trap_restore_idx].address );          
            // something changed, remove the BP
            gdbserver_state.breakpoints[trap_restore_idx].address = 0;
          }
          gdbserver_state.trap_flags &= (uint8_t)~TRAP_FLAG_RESTORE_RST08H;
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

            //extra check that there is an RST08 here (ie user didnt change it) - not foolproof but better than nothing
            if (*(uint8_t*)(gdbserver_state.breakpoints[i].address) == 0xCF)
            {
              *(uint8_t*)(gdbserver_state.breakpoints[i].address) = gdbserver_state.breakpoints[i].original_instruction;
              #ifdef DEBUG_BP              
                log("[REST INST  ] @ $", gdbserver_state.breakpoints[i].address );          
              #endif
            }
            else
            {
              log("[*ERR RESTIN] @ $", gdbserver_state.breakpoints[i].address );          
              // something changed, remove the BP
              gdbserver_state.breakpoints[i].address = 0;
              continue;
            }

            //Check if we hit the BP
            if (gdbserver_state.breakpoints[i].address == (rst08_address)) {
              trapped_breakpoint = i;
              gdbserver_state.trap_flags = (gdbserver_state.trap_flags | TRAP_FLAG_BREAK_HIT);
              log("[HIT      BP] @ $", gdbserver_state.breakpoints[i].address );          
            }
        }
      }
  }



  gdbserver_state.registers[REGISTERS_PC] = rst08_address; // address of breakpoint instr (RST 08)
  gdbserver_state.registers[REGISTERS_SP] = rst8_sp_copy  + 2;              // SP as it was before the trap (as the RST08 would have pushed its return address onto the stack)

  //read registers from stack order they are saved on stack is important !! see pcw_rst8.asm
  gdbserver_state.registers[REGISTERS_IY] = saved_registers[0];
  gdbserver_state.registers[REGISTERS_IX] = saved_registers[1];  
  gdbserver_state.registers[REGISTERS_HL] = saved_registers[2];
  gdbserver_state.registers[REGISTERS_DE] = saved_registers[3];
  gdbserver_state.registers[REGISTERS_BC] = saved_registers[4];
  gdbserver_state.registers[REGISTERS_AF] = saved_registers[5];

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
  saved_registers[0] = gdbserver_state.registers[REGISTERS_IY];
  saved_registers[1] = gdbserver_state.registers[REGISTERS_IX];
  saved_registers[2] = gdbserver_state.registers[REGISTERS_HL];
  saved_registers[3] = gdbserver_state.registers[REGISTERS_DE];
  saved_registers[4] = gdbserver_state.registers[REGISTERS_BC];
  saved_registers[5] = gdbserver_state.registers[REGISTERS_AF];

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
            #ifdef DEBUG_BP
              log("[SET BP     ] @ $", gdbserver_state.breakpoints[i].address );                    
            #endif
            
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

    // Used forced address from proprietory i instruction else use robust Z80 instruction decoder to find next instruction address
    uint16_t next_addr = ( gdbserver_state.trap_flags & TRAP_FLAG_FORCE_ADDRESS ) ? gdbserver_state.temporary_breakpoint.address :  calculateStep() ;
    
    // Clear the force address flag (if it was set) as we have now applied it
    gdbserver_state.trap_flags &= (uint8_t)~TRAP_FLAG_FORCE_ADDRESS;
    
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
  if (gdbserver_state.registers[REGISTERS_PC] != rst08_address) {
    // User changed PC in GDB, resume at new PC
    resume_addr = gdbserver_state.registers[REGISTERS_PC];
    log("                                        [CONT NEW PC] @ $", resume_addr );          
  } else if (gdbserver_state.trap_flags & TRAP_FLAG_BREAK_HIT ) {
    // Resume at the breakpoint address (original instruction restored)
    resume_addr = rst08_address;
    log("                                        [CONT       ] @ $", resume_addr );              
  } else {
    // Explicit RST 08 in user code: resume after RST 08
    resume_addr = rst08_return_address;
    log("                                        [CONT > RST8] @ $", resume_addr );                  
  }
  
//  rst8_sp_copy_base[6] = resume_addr;
  ((uint16_t*)rst8_sp_copy)[0] = resume_addr; // set return address on the stack 

  enable_serial_interrupt = 1;  //allow execution to be interrupted by serial 0x03 again

}
