#-------------------------------------------------------------------------
# VideoCore IV implementation of RGBtoHDMI
# (c) IanB Nov 2021
#-------------------------------------------------------------------------

# GPIO registers

.equ GPU_COMMAND,          0x7e0000a0  #use MBOX0-MBOX7 for ARM communications
.equ GPU_DATA_BUFFER_0,    0x7e0000a4
.equ GPU_DATA_BUFFER_1,    0x7e0000a8
.equ GPU_DATA_BUFFER_2,    0x7e0000ac
.equ GPU_SYNC,             0x7e0000b0  #gap in data block to allow fast 3 register read on ARM side
.equ GPU_DATA_BUFFER_3,    0x7e0000b4  #using a single ldr and a two register ldmia
.equ GPU_DATA_BUFFER_4,    0x7e0000b8  #can't use more than a single unaligned two register ldmia on the peripherals
.equ GPU_DATA_BUFFER_5,    0x7e0000bc

.equ GPU_COMMAND_offset,   0
.equ DATA_BUFFER_0_offset, 4
.equ DATA_BUFFER_1_offset, 8
.equ DATA_BUFFER_2_offset, 12
.equ GPU_SYNC_offset,      16
.equ DATA_BUFFER_3_offset, 20
.equ DATA_BUFFER_4_offset, 24
.equ DATA_BUFFER_5_offset, 28

.equ GPLEV0,          0x7e200034

.equ FINAL_BIT,            31             #signal if this sample word is the last
.equ PSYNC_BIT,            17             #alternates on each full 4 word buffer
.equ ODD_EVEN_BIT_HI,      16             #signal if low or high 16 bit sample is to be used
.equ ODD_EVEN_BIT_LO,      0              #signal if low or high 16 bit sample is to be used
.equ DEFAULT_BIT_STATE,    0x00020001     #FINAL_BIT=0, PSYNC_BIT=1, ODD_EVEN_BIT_HI=0, ODD_EVEN_BIT_LO=1
.equ MUX_BIT,              24             #video input for FFOSD
.equ ALT_MUX_BIT,          14             #moved version of MUX bit
.equ SYNC_BIT,             23             #sync input
.equ VIDEO_MASK,           0x3ffc         #12bit GPIO mask
.equ COMMAND_MASK,         0x00000fff     #masks out command bits that trigger sync detection

#macros

.macro LO_PSYNC_CAPTURE
wait_psync_lo\@:
   ld     r0, (r4)
   btst   r0, PSYNC_BIT
   bne    wait_psync_lo\@
   btst   r0, MUX_BIT
   and    r0, r6
   bsetne r0, ALT_MUX_BIT  #move mux bit to position in 16 bit sample
   or     r0, r2           #merge bit state
   sub    r3, 1
.endm

.macro HI_PSYNC_CAPTURE
wait_psync_hi\@:
   ld     r1, (r4)
   btst   r1, PSYNC_BIT
   beq    wait_psync_hi\@
   btst   r1, MUX_BIT
   and    r1, r6
   bsetne r1, ALT_MUX_BIT  #move mux bit to position in 16 bit sample
   lsl    r1, 16           #merge lo and hi samples
   or     r0, r1
   cmp    r3, 0
   bseteq r0, FINAL_BIT
.endm

.macro EDGE_DETECT
waitPSE\@:
   ld     r0, (r4)
   eor    r0, r2
   btst   r0, PSYNC_BIT
   bne    waitPSE\@
   eor    r0, r2       #restore r0 value
   bchg   r2, PSYNC_BIT
.endm

# main code entry point
   di
   cmp    r0, 1
   bne    not_gpio_read_benchmark
   mov    r2, 100000
   mov    r1, GPLEV0
read_bench_loop:
   ld     r3, (r1)  #read gpio
   sub    r2, 1
   cmp    r2, 0
   bne    read_bench_loop
   ei
   rts

not_gpio_read_benchmark:
   cmp    r0, 2
   bne    not_mbox_write_benchmark
   mov    r2, 100000
   mov    r1, GPU_DATA_BUFFER_5
   mov    r3, 0
write_bench_loop:
   st     r3, (r1)  #write to mbox
   sub    r2, 1
   cmp    r2, 0
   bne    write_bench_loop
   ei
   rts

not_mbox_write_benchmark:
   mov    r4, GPLEV0
   mov    r5, GPU_COMMAND
   mov    r6, VIDEO_MASK
   mov    r7, COMMAND_MASK
   mov    r8, DEFAULT_BIT_STATE
   mov    r2, 0x80000000   #default all samples with final bit set
   st     r2, DATA_BUFFER_0_offset(r5)
   st     r2, DATA_BUFFER_1_offset(r5)
   st     r2, DATA_BUFFER_2_offset(r5)
   st     r2, DATA_BUFFER_3_offset(r5)
   st     r2, DATA_BUFFER_4_offset(r5)
   st     r2, DATA_BUFFER_5_offset(r5)
   mov    r2, 0
   st     r2, GPU_SYNC_offset(r5)

wait_for_command:
   mov    r2, 0
   st     r2, GPU_COMMAND_offset(r5)                 #set command register to 0
   st     r2, GPU_SYNC_offset(r5)  #set sync register to 0
   mov    r2, r8                   #set the default state of the control bits

wait_for_command_loop:
   ld     r3, GPU_COMMAND_offset(r5)
   cmp    r3, 0
   beq    wait_for_command_loop
   btst   r3, 15         #bit signals upper 16 bits is a sync command
   beq    do_capture
   mov    r1, r3
   lsr    r1, 16

   #simple mode sync detection, enters with PSYNC_BIT set in r2
   cmp    r1, 0
   beq    edge_trail_neg
   cmp    r1, 1
   beq    edge_lead_neg
   bclr   r2, PSYNC_BIT       #only +ve edge (inverted later)
   cmp    r1, 2
   beq    edge_trail_pos
   cmp    r1, 3
   beq    edge_lead_pos
   cmp    r1, 4
   beq    edge_trail_both
   cmp    r1, 5
   bne    wait_for_command
   #if here then edge_lead_both

edge_lead_both:
   EDGE_DETECT
   btst   r0, SYNC_BIT
   bne    edge_lead_both
   st     r8, GPU_SYNC_offset(r5)   #lsbit flags sync detected
   b      done_simple_sync

edge_trail_both:
   EDGE_DETECT
   btst   r0, SYNC_BIT
   bne    edge_trail_both
   st     r8, GPU_SYNC_offset(r5)   #lsbit flags sync detected
edge_trail_both_hi:
   EDGE_DETECT
   btst   r0, SYNC_BIT
   beq    edge_trail_both_hi
   b      done_simple_sync

edge_lead_neg:
edge_lead_pos:
   #incoming psync state controls edge
wait_csync_lo2:
   EDGE_DETECT
   EDGE_DETECT
   btst   r0, SYNC_BIT
   bne    wait_csync_lo2
   st     r8, GPU_SYNC_offset(r5)   #lsbit flags sync detected
   b      done_simple_sync

edge_trail_neg:
edge_trail_pos:
   #incoming psync state controls edge *** this one used by amiga
wait_csync_lo:
   EDGE_DETECT
   EDGE_DETECT
   btst   r0, SYNC_BIT
   bne    wait_csync_lo
   st     r8, GPU_SYNC_offset(r5)   #lsbit flags sync detected
wait_csync_hi:
   EDGE_DETECT
   EDGE_DETECT
   btst   r0, SYNC_BIT
   beq    wait_csync_hi

done_simple_sync:
   btst   r2, PSYNC_BIT
   bne    no_compensate_psync
   EDGE_DETECT           #have to compensate because capture hard coded to always start on same edge
no_compensate_psync:

   mov    r2, r8         #set the default state of the control bits

do_capture:
   and    r3, r7         #mask off any command bits (max capture is 4095 psync cycles)
capture_loop:

   LO_PSYNC_CAPTURE
   HI_PSYNC_CAPTURE

   st     r0, DATA_BUFFER_0_offset(r5)
   beq    wait_for_command

   LO_PSYNC_CAPTURE
   HI_PSYNC_CAPTURE

   st     r0, DATA_BUFFER_1_offset(r5)
   beq    wait_for_command

   LO_PSYNC_CAPTURE
   HI_PSYNC_CAPTURE

   st     r0, DATA_BUFFER_2_offset(r5)
   beq    wait_for_command

   LO_PSYNC_CAPTURE
   HI_PSYNC_CAPTURE

   st     r0, DATA_BUFFER_3_offset(r5)
   beq    wait_for_command

   LO_PSYNC_CAPTURE
   HI_PSYNC_CAPTURE

   st     r0, DATA_BUFFER_4_offset(r5)
   beq    wait_for_command

   LO_PSYNC_CAPTURE
   bchg   r2, PSYNC_BIT        #invert the software psync bit every 12 samples / 6 words
   HI_PSYNC_CAPTURE

   st     r0, DATA_BUFFER_5_offset(r5)
   beq    wait_for_command

   b      capture_loop
