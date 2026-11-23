#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../defs.h"
#include "../startup.h"
#include "../logging.h"
#include "../cache.h"
#include "../filesystem.h"
#include "../rgb_to_hdmi.h"
#include "../rgb_to_fb.h"
#include "../rpi-mailbox-interface.h"
#include "../info.h"
#include "../rpi-gpio.h"
#include "start.h"
#include "hdmiaudio.h"
#include "hw.h"
#include "dma.h"

#define TABLE_MAX  100
static const uint16_t wav_table16[] = {
#include "480hz.h"
};
static uint32_t *sound_buffer = (uint32_t *) (UNCACHED_MEM_BASE + 0x1000000);
//static uint32_t *sound_buffer = (uint32_t *) (L2_CACHED_MEM_BASE + 0x1000000);

static uint32_t old_PLL_fraction = 0;
static uint32_t new_PLL_fraction = 0;

unsigned long audio_sample_rate_48000000 = 0;

int audio_hardware_type = AUDIO_NO_HARDWARE;
int sound_running = 0;

int get_audio_hardware_type() {
    return audio_hardware_type;
}

void initialise_audio_capture_on_VC1() {
#ifdef RPI4
    log_info("Audio not yet supported on Pi 4");
    RPI_SetGpioPinFunction(RPI_GPIO25, FS_OUTPUT); // mode 7 LED
#else
    log_info("Detecting audio hardware");
    SMI_DSR0[0] = 0;
    SMI_DSR0[1] = 0;
    //RPI_SetGpioPinFunction(RPI_GPIO14, FS_INPUT);

    RPI_SetGpioPinFunction(RPI_GPIO25, FS_INPUT); // mode 7 LED

    double cpuspeed = (double) get_cpuspeed();

    uint32_t * audio_timing = measure_audio_clock();

    uint32_t audio_clock_rate = (double) audio_timing[0] * cpuspeed * 1000000 / AUDIO_TEST_DURATION;
    uint32_t audio_data1_rate = (double) audio_timing[1] * cpuspeed * 1000000 / AUDIO_TEST_DURATION;
    uint32_t audio_data2_rate = (double) audio_timing[2] * cpuspeed * 1000000 / AUDIO_TEST_DURATION;

    uint32_t audio_data1_pin = 0;
    uint32_t audio_data2_pin = 0;
    uint32_t sample_repeat = 0;

    if (audio_clock_rate > (23900 * 64) && audio_clock_rate < (24100 * 64)) {
        if (audio_data1_rate > (23900 * 2) && audio_data1_rate < (24100 * 2)) {
            audio_data1_pin = AUDIO_DATA2_PIN;
            audio_data2_pin = AUDIO_DATA1_PIN;
            sample_repeat = 1;
            audio_sample_rate_48000000 = 2 * (double) audio_clock_rate * 1000 / 64;
            audio_hardware_type = AUDIO_2GPIO_24KHZ_L;
        } else if (audio_data2_rate > (23900 * 2) && audio_data2_rate < (24100 * 2)) {
            audio_data1_pin = AUDIO_DATA1_PIN;
            audio_data2_pin = AUDIO_DATA2_PIN;
            sample_repeat = 1;
            audio_sample_rate_48000000 = 2 * (double) audio_clock_rate * 1000 / 64;
            audio_hardware_type = AUDIO_2GPIO_24KHZ_F;
        } else {
            audio_data1_pin = 0;
            audio_data2_pin = 0;
            sample_repeat = 0;
            audio_sample_rate_48000000 = 0;
        }
    } else if (audio_clock_rate > (47900 * 64) && audio_clock_rate < (48100 * 64)) {
        if (audio_data1_rate > (47900 * 2) && audio_data1_rate < (48100 * 2)) {
            audio_data1_pin = AUDIO_DATA2_PIN;
            audio_data2_pin = AUDIO_DATA1_PIN;
            sample_repeat = 0;
            audio_sample_rate_48000000 = (double) audio_clock_rate * 1000 / 64;
            audio_hardware_type = AUDIO_2GPIO_48KHZ_L;
        } else if (audio_data2_rate > (47900 * 2) && audio_data2_rate < (48100 * 2)) {
            audio_data1_pin = AUDIO_DATA1_PIN;
            audio_data2_pin = AUDIO_DATA2_PIN;
            sample_repeat = 0;
            audio_sample_rate_48000000 = (double) audio_clock_rate * 1000 / 64;
            audio_hardware_type = AUDIO_2GPIO_48KHZ_F;
        } else {
            if ((audio_data2_rate > 47900 && audio_data2_rate < (96100 * 64)) || audio_data1_rate < 23900 ) {
                audio_data1_pin = AUDIO_DATA2_PIN;
                audio_data2_pin = AUDIO_DATA2_PIN;
                sample_repeat = 1;
                audio_sample_rate_48000000 = (double) audio_clock_rate * 1000 / 64;
                audio_hardware_type = AUDIO_1GPIO_24KHZ_L;
            } else {
                audio_data1_pin = AUDIO_DATA1_PIN;
                audio_data2_pin = AUDIO_DATA1_PIN;
                sample_repeat = 1;
                audio_sample_rate_48000000 = (double) audio_clock_rate * 1000 / 64;
                audio_hardware_type = AUDIO_1GPIO_24KHZ_F;
            }
        }
    } else if (audio_clock_rate > (95900 * 64) && audio_clock_rate < (96100 * 64)) {
            if ((audio_data2_rate > 95900 && audio_data2_rate < (96100 * 64)) || audio_data1_rate < 23900 ) {
                audio_data1_pin = AUDIO_DATA2_PIN;
                audio_data2_pin = AUDIO_DATA2_PIN;
                sample_repeat = 0;
                audio_sample_rate_48000000 = (double) audio_clock_rate * 1000 / 64 / 2;
                audio_hardware_type = AUDIO_1GPIO_48KHZ_L;
            } else {
                audio_data1_pin = AUDIO_DATA1_PIN;
                audio_data2_pin = AUDIO_DATA1_PIN;
                sample_repeat = 0;
                audio_sample_rate_48000000 = (double) audio_clock_rate * 1000 / 64 / 2;
                audio_hardware_type = AUDIO_1GPIO_48KHZ_F;
            }
    } else {
        audio_data1_pin = 0;
        audio_data2_pin = 0;
        sample_repeat = 0;
        audio_sample_rate_48000000 = 0;
    }
    if (audio_sample_rate_48000000 != 0) {
        if (audio_data2_pin == audio_data1_pin) {
            log_info("Capture sample rate of %dHz, 1 GPIO mode detected: Data1 = %d, Data2 = %d", audio_sample_rate_48000000 / 1000 / (sample_repeat + 1), audio_data1_pin, audio_data2_pin );
        } else {
            log_info("Capture sample rate of %dHz, 2 GPIO mode detected: Data1 = %d, Data2 = %d", audio_sample_rate_48000000 / 1000 / (sample_repeat + 1), audio_data1_pin, audio_data2_pin );
        }
        static volatile uint32_t *gpioreg;
        gpioreg = (volatile uint32_t *)(_get_peripheral_base() + 0x101000UL);

        old_PLL_fraction = gpioreg[PLLD_FRAC];

        log_info("HDMI playback sample rate %f (nominally 48000)", (double) (audio_sample_rate_48000000) / 1000);
        unsigned int  new_freq = (unsigned int) (double) 2000000000.0f * (double)(audio_sample_rate_48000000)/ 48000000;
        log_info("New PLLD frequency %dHz (nominally 2000000000Hz)", new_freq );

        set_pll_frequency(((double) (new_freq >> 1)) / 1e6, PLLD_CTRL, PLLD_FRAC);

        new_PLL_fraction = gpioreg[PLLD_FRAC];

        log_info("PLL: normal = 0x%08X, adjusted = 0x%08X", old_PLL_fraction, new_PLL_fraction);

        log_info("Starting VC1 core...");
        SMI_DSR0[0] = 0; //command register (standby)
        SMI_DSR0[1] = 1; //status register (busy)
        SMI_DSR0[2] = (uint32_t) HDMI_MAI_DATA_BUS; //buffer pointer
        SMI_DSR0[3] = (uint32_t) HDMI_MAI_DATA_BUS; //buffer start
        SMI_DSR0[4] = (uint32_t) HDMI_MAI_DATA_BUS; //buffer end
        SMI_DSR0[5] = 0;
        SMI_DSR0[6] = 0;
        SMI_DSR0[7] = 0;
        start_vc_1(audio_data1_pin, audio_data2_pin, sample_repeat,0);
        do {
        } while (SMI_DSR0[1] !=0);
        log_info("VC1 core running and ready for audio commands");

        set_vc_irq_vectors();

    } else {
        log_info("No valid audio interface detected");
        SMI_DSR0[0] = 0; //command register (standby)
        SMI_DSR0[1] = 0; //status register (busy)
        SMI_DSR0[2] = (uint32_t) HDMI_MAI_DATA_BUS; //buffer pointer
        SMI_DSR0[3] = (uint32_t) HDMI_MAI_DATA_BUS; //buffer start
        SMI_DSR0[4] = (uint32_t) HDMI_MAI_DATA_BUS; //buffer end
        SMI_DSR0[5] = 0;
        SMI_DSR0[6] = 0;
        SMI_DSR0[7] = 0;

    }

    RPI_SetGpioPinFunction(RPI_GPIO25, FS_OUTPUT); // mode 7 LED
#endif
}



void start_hdmi_sound(uint32_t *buffer, uint32_t size) {
    log_info("Starting audio, size = %d bytes", size);
    //hdmi_print_regs();
    //hd_print_regs();
    hdmi_audio_setup(48000000);
    hdmi_print_regs();
    hd_print_regs();
    start_sound_dma(buffer, size);
    dma_debug(0);
    log_info("Audio setup complete");
}

void stop_audio_capture() {
    if (audio_hardware_type != AUDIO_NO_HARDWARE) {
        log_info("Stopping audio capture...");
        SMI_DSR0[0] = 0; //command register (standby)
        do {
        } while ((SMI_DSR0[1] & 1) !=0);
        log_info("Audio capture stopped.");
    }
     RPI_SetGpioPinFunction(RPI_GPIO25, FS_OUTPUT); // mode 7 LED
}

void stop_all_audio() {
    if (sound_running) {
        //todo investigate proper dma shutdown
        stop_dma(0);
        dma_init();
        stop_audio_capture();
        hdmi_audio_reset();
        sound_running = 0;
    }
}


void start_audio_capture() {
    if (audio_hardware_type != AUDIO_NO_HARDWARE) {
        stop_all_audio();
        RPI_SetGpioPinFunction(RPI_GPIO25, FS_INPUT); // mode 7 LED

        uint32_t buffer_start;
        uint32_t buffer_size;
        uint32_t buffer_end;
        uint32_t buffer_pointer;
        uint32_t topbits = get_GPU_top_bits();

        static volatile uint32_t *gpioreg;
        gpioreg = (volatile uint32_t *)(_get_peripheral_base() + 0x101000UL);

        if (get_parameter(F_CLOCK_SYNC) == CLOCK_SAMPLES) {
            gpioreg[PLLD_FRAC] = old_PLL_fraction | CM_PASSWORD ;
            delay_in_arm_cycles_cpu_adjust(1000000);
            hdmi_audio_setup(48000000);
        } else {
            gpioreg[PLLD_FRAC] = new_PLL_fraction | CM_PASSWORD ;
            delay_in_arm_cycles_cpu_adjust(1000000);
            hdmi_audio_setup(audio_sample_rate_48000000);
        }
        log_info("Audio new PLL = 0x%08X", gpioreg[PLLD_FRAC]);

        if (get_parameter(F_DMA)) {
            buffer_start = (uint32_t) sound_buffer | topbits;
            buffer_size = get_parameter(F_DMA_DELAY) * 2 * 48000 * 2 *  4 / 1000; // ? milliseconds * 2 * 48000 samples per second * 2 channels * 4 bytes per sample / 1000 ms in a second
            buffer_pointer = buffer_start + (buffer_size >> 1);
            buffer_end = buffer_start + buffer_size;

            for (int i = 0; i< buffer_size /4; i++) {
                sound_buffer[i] = 0x12345678;
            }

            SMI_DSR0[2] = gpioreg[PLLD_FRAC];
            SMI_DSR0[3] = buffer_start; //buffer start
            SMI_DSR0[4] = buffer_end; //buffer end
            log_info("Starting audio using DMA capture: PTR=0x%08X, Start=0x%08X, End=0x%08X", buffer_pointer, buffer_start, buffer_end);
            start_hdmi_sound(sound_buffer, SMI_DSR0[4] - SMI_DSR0[3]);
            delay_in_arm_cycles_cpu_adjust(1000000); //wait a quarter of the buffer size to allow some preload by dma  get_parameter(F_DMA_DELAY) * 1000000 / 4
            dma_debug(0);
        } else {
            SMI_DSR0[2] = gpioreg[PLLD_FRAC];
            SMI_DSR0[3] = (uint32_t) HDMI_MAI_DATA_BUS; //buffer start
            SMI_DSR0[4] = (uint32_t) HDMI_MAI_DATA_BUS; //buffer end
            log_info("Starting audio using direct HDMI_MAI_DATA_BUS register capture");
        }


        SMI_DSR0[0] = 1 | ((get_parameter(F_CLOCK_SYNC)) << 1) | ((get_parameter(F_DMA)) << 3) | ((get_parameter(F_AUDIO_MODE)) << 4); //set run bit in command register

        do {  //wait until running
        } while ((SMI_DSR0[1] & 1) == 0);

        sound_running = 1;

        log_info("Audio capture setup complete");
 /*
        static volatile uint32_t *gpioreg;
        gpioreg = (volatile uint32_t *)(_get_peripheral_base() + 0x101000UL);
        uint32_t smi = 0;
        uint32_t frac = 0;
        uint32_t oldsmi = 0;
        uint32_t oldfrac = 0;
        do {
            smi = SMI_DSR0[6];
            frac = gpioreg[PLLD_FRAC];
            if (smi != oldsmi || frac != oldfrac) {
                log_info("%04X, %06X", smi, frac);
                oldsmi = smi;
                oldfrac = frac;
            }
        }while(1);
        */

        /*
        do {
            log_info("200: %08X", INT_BASE[0]);
            log_info("204: %08X", INT_BASE[4]);
            log_info("208: %08X", INT_BASE[8]);
            log_info("20c: %08X", INT_BASE[0x0c]);
            log_info("210: %08X", INT_BASE[0x10]);
            log_info("214: %08X", INT_BASE[0x14]);
            log_info("218: %08X", INT_BASE[0x18]);
            log_info("21c: %08X", INT_BASE[0x1c]);
            log_info("220: %08X", INT_BASE[0x20]);
            log_info("224: %08X", INT_BASE[0x24]);
             log_info("");
        }while(1);
*/

    } else {
        set_status_message("No Audio Capture Hardware Detected");
        RPI_SetGpioPinFunction(RPI_GPIO25, FS_OUTPUT); // mode 7 LED
    }
}

void set_audio_capture(int state) {
    if (state) {
        start_audio_capture();
    } else {
        stop_all_audio();
    }

}

//void update_audio_ctrl() {
//    if (audio_hardware_type != AUDIO_NO_HARDWARE) {
//        int audio_running = SMI_DSR0[0] & 1;
//        if (audio_running){
//            start_audio_capture();
//        }
//    }
//}

void start_tone() {
    stop_all_audio();
    int sound_buffer_size = 0;
    log_info("Generating Test Tone");
    for (int count = 0; count < 64; count++) {
        for (int sample = 0; sample < TABLE_MAX; sample++) {
            uint32_t data = wav_table16[sample];
            sound_buffer[sound_buffer_size++] = ConvertIEC958Sample16(data);
            sound_buffer[sound_buffer_size++] = ConvertIEC958Sample16(data);
        }
    }
    start_hdmi_sound(sound_buffer, sound_buffer_size << 2);
    sound_running = 1;
}

void start_audio(char* filename) {
    stop_all_audio();
    unsigned int sound_buffer_size = 0;
    char path[MAX_STRING_SIZE];
    sprintf(path, "/WAVs/%s.wav", filename);
    sound_buffer_size = file_load_WAV(path, sound_buffer);
    start_hdmi_sound(sound_buffer, sound_buffer_size << 2);
    sound_running = 1;
}

void start_all_audio(char wav_names[MAX_NAMES][MAX_NAMES_WIDTH], int count) {
    stop_all_audio();
    unsigned int sound_buffer_size = 0;
    char path[MAX_STRING_SIZE];
    for (int i = 0; i < count; i++) {
        sprintf(path, "/WAVs/%s.wav", wav_names[i]);
        unsigned int size = file_load_WAV(path, sound_buffer + sound_buffer_size);
        sound_buffer_size += size;
        if (size != 0) {
            for (int j = 0; j < 24000; j++) {         //add 0.5sec silence between wavs
                sound_buffer[sound_buffer_size++] = ConvertIEC958Sample16(0);
                sound_buffer[sound_buffer_size++] = ConvertIEC958Sample16(0);
            }
        }
    }
    start_hdmi_sound(sound_buffer, sound_buffer_size << 2);
    sound_running = 1;
}
