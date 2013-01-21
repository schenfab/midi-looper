/**************************************************************************
 * alsa_midi.c
 *
 * Lists sound cards. Picks one specific midi input / output pair and
 * prints received data to the console.
 *
 * Compilation: gcc -Wall -o alsa_midi alsa_midi.c -lasound
 **************************************************************************/

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>


/**************************************************************************
 * Function prototypes
 **************************************************************************/
void print_help(char *app_name);
void list_cards();
void print_midi_to_console(char *card_num_str);
void play_test_sound(char *card_num_str);


/**************************************************************************
 * Routines
 **************************************************************************/
int main(int argc, char **argv)
{
    // Command line parsing
    if(argc == 2 && strcmp(argv[1], "-l") == 0) {
        list_cards();
    }
    else if(argc == 3 && strcmp(argv[1], "-p") == 0) {
        print_midi_to_console(argv[2]);
    }
    else if(argc == 3 && strcmp(argv[1], "-t") == 0) {
        play_test_sound(argv[2]);
    }
    else {
        print_help(argv[0]);
    }

    return 0;
}


void print_help(char *app_name) {
    printf("Usage: %s [ARGS]\n"                                         \
           "\n"                                                         \
           "ARGS:\n"                                                    \
           "   -h         Print this help\n"                            \
           "   -l         List cards\n"                                 \
           "   -p <NAME>  Print midi input of port <NAME> to console\n" \
           "   -t <NAME>  Play test sound to port <NAME> "              \
           "\n",
           app_name);
}


void list_cards() {
    // Variables
    int err;
    int card_num;
    snd_ctl_t *card_handle;
    snd_ctl_card_info_t *card_info;
    char card_str[64];
    snd_rawmidi_info_t *raw_midi_info;
    int dev_num;
    int sub_dev_num;
    int sub_dev_cnt;

    // Start with first card
    card_num = -1;

    // Loop over cards
    for(;;) {
        // Get next sound card. When "card_num" == -1, then ALSA 
        // fetches the first card
        if((err = snd_card_next(&card_num)) < 0) {
            printf("ERROR: Can't get the next card number: %s\n", snd_strerror(err));
            break;
        }

        // ALSA sets "card_num" to -1 if no more card has been fetched
        if(card_num == -1) break;
        
        // Open the card's control interface
        sprintf(card_str, "hw:%i", card_num);
        if((err = snd_ctl_open(&card_handle, card_str, 0)) < 0) {
            printf("ERROR: Can't open card %i: %s\n", card_num, snd_strerror(err));
            continue;
        }
        
        // Get card name
        snd_ctl_card_info_alloca(&card_info);
        if((err = snd_ctl_card_info(card_handle, card_info)) < 0) {
            printf("ERROR: Can't get info for card %i: %s\n", card_num, snd_strerror(err));;
        }
        else {
            printf("Card %i = %s\n", card_num, snd_ctl_card_info_get_name(card_info));
        }

        // Loop through the MIDI devices, start with first device
        dev_num = -1;
        for(;;) {
            // Get next midi device on the card
            if((err = snd_ctl_rawmidi_next_device(card_handle, &dev_num)) < 0) {
                printf("ERROR: Can't get next MIDI device number: %s\n", snd_strerror(err));
                break;
            }

            // ALSA sets "card_num" to -1 if no more interface has been fetched
            if(dev_num == -1) break;

            // Get info about subdevices
            snd_rawmidi_info_alloca(&raw_midi_info);
            memset(raw_midi_info, 0, snd_rawmidi_info_sizeof());
            snd_rawmidi_info_set_device(raw_midi_info, dev_num);

            // First INPUTS ...
            snd_rawmidi_info_set_stream(raw_midi_info, SND_RAWMIDI_STREAM_INPUT);

            // Loop through subdevices, start with first device
            sub_dev_num = -1;
            sub_dev_cnt = 1; // Temporary value for first loop cycle
            while(++sub_dev_num < sub_dev_cnt) {
                // Get info on subdevice
                snd_rawmidi_info_set_subdevice(raw_midi_info, sub_dev_num);
                if((err = snd_ctl_rawmidi_info(card_handle, raw_midi_info)) < 0) {
                    printf("ERROR: Can't get info for MIDI output subdevice hw:%i,%i,%i: %s\n",
                           card_num, dev_num, sub_dev_num, snd_strerror(err));
                    continue;
                }
                
                // Get total number of subdevices (only once)
                if(sub_dev_num == 0) {
                    sub_dev_cnt = snd_rawmidi_info_get_subdevices_count(raw_midi_info);
                }

                // Print device string
                printf("  MIDI In  %i = hw:%i,%i,%i\n", sub_dev_num, card_num, dev_num, sub_dev_num);
            }

            // ... then OUTPUTS
            snd_rawmidi_info_set_stream(raw_midi_info, SND_RAWMIDI_STREAM_OUTPUT);

            // Loop through subdevices, start with first device
            sub_dev_num = -1;
            sub_dev_cnt = 1; // Temporary value for first loop cycle
            while(++sub_dev_num < sub_dev_cnt) {
                // Get info on subdevice
                snd_rawmidi_info_set_subdevice(raw_midi_info, sub_dev_num);
                if((err = snd_ctl_rawmidi_info(card_handle, raw_midi_info)) < 0) {
                    printf("ERROR: Can't get info for MIDI output subdevice hw:%i,%i,%i: %s\n",
                           card_num, dev_num, sub_dev_num, snd_strerror(err));
                    continue;
                }
                
                // Get total number of subdevices (only once)
                if(sub_dev_num == 0) {
                    sub_dev_cnt = snd_rawmidi_info_get_subdevices_count(raw_midi_info);
                }

                // Print device string
                printf("  MIDI Out %i = hw:%i,%i,%i\n", sub_dev_num, card_num, dev_num, sub_dev_num);
            }
        }

        // Close card
        snd_ctl_close(card_handle);
    }

    // Free all alsa memory
    snd_config_update_free_global();
}


void print_midi_to_console(char *card_num_str) {
    // Variables
    int err;
    unsigned char buf;
    snd_rawmidi_t *midi_in_handle;

    // Open the midi interface
    if((err = snd_rawmidi_open(&midi_in_handle, NULL, card_num_str, 0)) < 0) {
        printf("ERROR: Can't open MIDI input '%s': %s\n", card_num_str, snd_strerror(err));
        return;
    }

    while(1) {
        if((err = snd_rawmidi_read(midi_in_handle, &buf, 1)) < 0) {
            printf("ERROR: Can't read MIDI input '%s': %s\n", card_num_str, snd_strerror(err));
            return;
        }
        printf("Midi in: 0x%X\n", buf);
    }

    // Close midi interface
    if((err = snd_rawmidi_close(midi_in_handle)) < 0) {
        printf("ERROR: Can't close MIDI input '%s': %s\n", card_num_str, snd_strerror(err));
        return;
    }    
}


void play_test_sound(char *card_num_str) {
    // Variables
    int i, err;
    unsigned char tones[] = {60, 64, 67, 72, 67, 64, 60};
    unsigned char lengths[] = {1, 1, 1, 1, 1, 1, 2};
    unsigned char buf[3];
    snd_rawmidi_t *midi_out_handle;

    // Open the midi interface
    if((err = snd_rawmidi_open(NULL, &midi_out_handle, card_num_str, 0)) < 0) {
        printf("ERROR: Can't open MIDI output '%s': %s\n", card_num_str, snd_strerror(err));
        return;
    }

    // Play test note
    for(i = 0; i < 7; i++) {
        buf[0] = 0x90;      // Note on, channel 0
        buf[1] = tones[i];  // Tone
        buf[2] = 0x7F;      // Velocity
        snd_rawmidi_write(midi_out_handle, buf, 3);
        snd_rawmidi_drain(midi_out_handle);
        
        usleep(250000*lengths[i]);
        
        buf[2] = 0;         // Velocity of 0 results in note off
        snd_rawmidi_write(midi_out_handle, buf, 3);
        snd_rawmidi_drain(midi_out_handle);
    }

    // Close midi interface
    if((err = snd_rawmidi_close(midi_out_handle)) < 0) {
        printf("ERROR: Can't close MIDI output '%s': %s\n", card_num_str, snd_strerror(err));
        return;
    }    
}

/**************************************************************************
 * EOF
 **************************************************************************/
