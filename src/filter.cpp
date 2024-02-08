#include <cinttypes>
#include <iostream>
#include <format>
#include <cups/cups.h>
#include <cups/raster.h>
#include <cups/ppd.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>

static uint global_is_cancelled;
static uint global_page;
static uint global_model_number;
static uint global_feed;
static uint global_last_set;
static void * global_buffer;
static void * global_compress_buffer;
static void * global_last_buffer;
static uint global_set;

static const char * SCHEDULER_ERROR = "ERROR";
static const char * SCHEDULER_INFO = "ERROR";
static const char * SCHEDULER_PAGE = "PAGE";
static const char * SCHEDULER_ATTR = "ATTR";

void
cancel_job()
{
    global_is_cancelled = 1;
}

void
signal_handler(int sig)
{
    cancel_job();
}

void
message_scheduler(
    std::string type,
    std::string message)
{
    std::cerr << type << ": " << message << std::endl;
}

void
log(std::string s)
{
    std::cerr << "DRU FILTER: " << s << std::endl;
    std::fflush(stderr);
}

void
Setup(ppd_file_t * ppd_file)
{
    int model_number = global_model_number;

    // Update Model Number
    if (ppd_file)
    {
        log(std::format("PPD Model number: {}", ppd_file->model_number));
        model_number = ppd_file->model_number;
        global_model_number = model_number;
    }

    // Custom Setup
    if (model_number == 0x20)
    {
        putchar(0x1B);
        putchar(0x45);
        return;
    }
    else if (!model_number)
    {
        for (int i = 0; i < 100; i++)
        {
            putchar(0x1B);
        }

        fputs("\x1B""@", stdout);
    }
}

void
StartPage(
    ppd_file_t * ppd_file,
    cups_page_header2_t * header)
{
    log("Inside StartPage");
    switch (global_model_number)
    {
        case 0x10:
        {
            ppd_choice_t * ze_print_rate = ppdFindMarkedChoice(ppd_file, "zePrintRate");
            if (ze_print_rate)
            {
                bool is_not_default = strcmp(ze_print_rate->choice, "Default");
                if (is_not_default)
                {
                    float print_rate = atof(ze_print_rate->choice);
                    printf("\x1bS%.0f", print_rate + print_rate - 2.0);
                }
            }
            if (header->cupsCompression - 1 < 100)
            {
                ulong new_compression = ((ulong)(header->cupsCompression * 7) & 0xFFFF) >> 2;
                new_compression = (ulong)((uint)(new_compression) / 0x19);
                printf("\x1b""D%lu", new_compression);
            }
            fputs("\x1b""M01", stdout);
            fputs("\x1b""B", stdout);
            global_buffer = malloc((ulong) header->cupsBytesPerLine);
            global_feed = 0;
        } break;

        case 0x11:
        {
            putchar(0x0A);
            puts("N");
            bool is_direct = strcmp(header->MediaType, "Direct") == 0;
            if (is_direct)
            {
                puts("OD");
            }

            ppd_choice_t * ze_print_rate = ppdFindMarkedChoice(ppd_file, "zePrintRate");
            if (ze_print_rate)
            {
                bool is_not_default = strcmp(ze_print_rate->choice, "Default");
                if (is_not_default)
                {
                    float print_rate = atof(ze_print_rate->choice);
                    if (print_rate < 3.0)
                    {
                        print_rate = print_rate + print_rate - 2.0;
                    }

                    printf("S%.0f\n", print_rate);
                }
            }
            if (header->cupsCompression - 1 < 100)
            {
                printf("D%lu\n", (ulong)(((header->cupsCompression * 0xf) & 0xFFFF) / 100));
            }
            printf("q%lu\n", (ulong)((header->cupsWidth + 7) & 0xFFFFFFF8));
            global_buffer = malloc((ulong) header->cupsBytesPerLine);
            global_feed = 0;
        } break;

        case 0x12:
        {
            if (header->cupsCompression - 1 < 100)
            {
                ulong new_compression = ((ulong)(header->cupsCompression * 0x1e) & 0xFFFF) >> 2;
                new_compression = (ulong)((uint)(new_compression) / 0x19);
                printf("~SD%02lu\n", new_compression);
            }
            printf("~DGR:CUPS.GRF,%u,%u,\n",
                    header->cupsHeight * header->cupsBytesPerLine,
                    header->cupsBytesPerLine);
            global_compress_buffer = malloc(header->cupsBytesPerLine * 2 + 1);
            global_last_buffer = malloc(header->cupsBytesPerLine);
            global_buffer = malloc((ulong) header->cupsBytesPerLine);
            global_feed = 0;
        } break;

        case 0x13:
        {
            printf("! 0 %u %u %u %u\r\n",
                    header->HWResolution[0],
                    header->HWResolution[1],
                    header->cupsHeight,
                    header->NumCopies);

            printf("PAGE-WIDTH %u\r\n", header->cupsWidth);
            printf("PAGE-HEIGHT %u\r\n", header->cupsHeight);
            global_buffer = malloc((ulong) header->cupsBytesPerLine);
            global_feed = 0;
        } break;

        case 0x14:
        {
            int adjust_horizontal = 0;
            int adjust_vertical = 0;

            printf("SIZE %u mm ,%u mm\n",
                    ((header->cupsWidth + 7) >> 3),
                    ((header->cupsHeight + 7) >> 3));

            log("Getting adjusts");
            ppd_choice_t * choice_adjust_horizontal = ppdFindMarkedChoice(ppd_file,"AdjustHoriaontal");
            if (choice_adjust_horizontal)
            {
                adjust_horizontal = atoi(choice_adjust_horizontal->choice) << 3;
            }

            ppd_choice_t * choice_adjust_vertical = ppdFindMarkedChoice(ppd_file,"AdjustVertical");
            if (choice_adjust_vertical)
            {
              adjust_vertical = atoi(choice_adjust_vertical->choice) << 3;
            }
            printf("REFERENCE %d,%d\n",
                    adjust_horizontal,
                    adjust_vertical);

            log("rotation");
            int rotate = 0;
            ppd_choice_t * choice_rotate = ppdFindMarkedChoice(ppd_file,"Rotate");
            if (choice_rotate)
            {
                rotate = atoi(choice_rotate->choice);
            }
            printf("DIRECTION %d,0\n", rotate);
            puts("GAP 3 mm,0 mm");

            adjust_vertical = 0;
            if (choice_adjust_vertical)
            {
                adjust_vertical = atoi(choice_adjust_vertical->choice);
            }
            printf("OFFSET %d mm\n", adjust_vertical);

            ppd_choice_t * choice_darkness = ppdFindMarkedChoice(ppd_file,"Darkness");
            if (choice_rotate)
            {
                if (strcmp(choice_darkness->choice, "Default"))
                {
                    printf("DENSITY %d\n", atoi(choice_darkness->choice));
                }
            }

            log("print rate");
            ppd_choice_t * choice_ze_print_rate = ppdFindMarkedChoice(ppd_file,"zePrintRate");
            if (choice_ze_print_rate)
            {
                if (strcmp(choice_ze_print_rate->choice, "Default"))
                {
                    printf("SPEED %d\n", atoi(choice_ze_print_rate->choice));
                }
            }

            ppd_choice_t * choice_auto_dotted = ppdFindMarkedChoice(ppd_file,"AutoDotted");
            if (!choice_auto_dotted || atoi(choice_auto_dotted->choice) == 0)
            {
                puts("SETC AUTODOTTED OFF");
            }
            else
            {
                puts("SETC AUTODOTTED ON");
            }
            puts("SETC PAUSEKEY ON");
            puts("SETC WATERMARK OFF");
            puts("CLS");
            printf("BITMAP 0,0,%u,%u,1,",
                    (header->cupsWidth + 7) >> 3,
                    header->cupsHeight);

            log("mallocing");
            global_buffer = malloc((ulong) header->cupsBytesPerLine);
            global_feed = 0;
        } break;

        case 0x20:
        {
            printf("\x1b""&l6D""\x1b""&k12H");
            printf("\x1b""&l0O");
            int page_size = header->PageSize[1];

            switch (page_size)
            {
                case 0x21C:
                {
                    printf("\x1b""&l80A");
                } break;

                case 0x270:
                {
                    printf("\x1b""&l90A");
                } break;
                case 0x289:
                {
                    printf("\x1b""&l91A");
                } break;

                case 0x2AC:
                {
                    printf("\x1b""&l81A");
                } break;

                case 0x2F4:
                {
                    printf("\x1b""&l1A");
                } break;

                case 0x318:
                {
                    printf("\x1b""&l2A");
                } break;

                case 0x34A:
                {
                    printf("\x1b""&l26A");
                } break;

                case 0x3F0:
                {
                    printf("\x1b""&l3A");
                } break;

                default:
                {
                    printf("\x1b""!f%uZ", page_size * 300 / 0x48);
                } break;
            }

            printf("\x1b""&l%uP",header->PageSize[1] / 0xc);
            printf("\x1b""&l0E");

            if (header->NumCopies != 0)
            {
                printf("\x1b""&l%uX", header->NumCopies);
            }
            printf("\x1b""&l0L");

            if (global_page == 1)
            {
                if (header->cupsRowFeed != 0)
                {
                    printf("\x1b""!p%uS", header->cupsRowFeed);
                }

                if (header->cupsCompression != -1)
                {
                    printf("\x1b""&d%dA",(int)((header->cupsCompression * 0x1e) / 100 - 0xf));
                }

                ppd_choice_t * choice_in_print_mode = ppdFindMarkedChoice(ppd_file,"inPrintMode");
                if (choice_in_print_mode)
                {
                    bool is_standard = strcmp(choice_in_print_mode->choice, "Standard") == 0;
                    bool is_tear = strcmp(choice_in_print_mode->choice, "Tear") == 0;
                    if (is_standard)
                    {
                        fputs("\x1b!p0M", stdout);
                    }
                    else if (is_tear)
                    {
                        fputs("\x1b!p1M", stdout);
                        if (header->cupsRowCount != 0)
                        {
                            printf("\x1b!n%uT", header->cupsRowCount);
                        }
                    }
                    else
                    {
                        fputs("\x1b!p2M", stdout);
                        if (header->cupsRowStep != 0)
                        {
                            printf("\x1b!n%uC", header->cupsRowStep);
                        }
                    }
                }
            }
            printf("\x1b*t%uR",(unsigned int)(ulong)header->HWResolution[0]);
            printf("\x1b*r%uS",(unsigned int)(ulong)header->cupsWidth);
            printf("\x1b*r%uT",(unsigned int)(ulong)header->cupsHeight);
            printf("\x1b&a0H");
            printf("\x1b&a0V");
            printf("\x1b*r1A");
            printf("\x1b*b3M");
            global_compress_buffer = malloc(header->cupsBytesPerLine * 2 + 1);
            global_last_buffer = malloc(header->cupsBytesPerLine);
            global_buffer = malloc((ulong) header->cupsBytesPerLine);
            global_feed = 0;
        } break;

        default:
        {
            if (global_model_number != 0)
            {
                global_buffer = malloc((ulong) header->cupsBytesPerLine);
                global_feed = 0;
            }
            else // model_number == 0
            {
                uint uVar4 = (ulong)(header->HWResolution[1] * header->PageSize[1]);
                printf("\x1b""L%c%c", uVar4 / 0x4800, uVar4 / 0x48);
                printf("\x1b""D%c",(int)header->cupsBytesPerLine);
                printf("\x1b""%c", (int)(header->cupsCompression + 99));
                global_buffer = malloc((ulong) header->cupsBytesPerLine);
                global_feed = 0;
            }
        } break;
    }
}

void
EndPage(
    ppd_file_t * ppd_file,
    cups_page_header2_t * header)
{
    switch (global_model_number)
    {
        case 0x10:
        {
            fputs("\x1B""E\f", stdout);
        } break;

        case 0x11:
        {
            puts("P1");
            if (header->CutMedia != CUPS_CUT_NONE)
            {
                puts("C");
            }
        } break;

        case 0x12:
        {
            if (global_is_cancelled)
            {
                puts("~DN");
            }
            else
            {
                puts("^XA");
                puts("^POI");
                printf("^PW%u\n", (unsigned int) header->cupsWidth);
                ppd_choice_t * choice_print_rate = ppdFindMarkedChoice(ppd_file, "zePrintRate");
                if (choice_print_rate)
                {
                    bool is_default = strcmp(choice_print_rate->choice, "Default") == 0;
                    if (!is_default)
                    {
                        unsigned int print_rate = atoi(choice_print_rate->choice);
                        printf("^PR%d,%d,%d\n", print_rate, print_rate, print_rate);
                    }
                }

                puts("^LH0,0");

                if (ppdIsMarked(ppd_file, "zeMediaTracking", "Continuous"))
                {
                    printf("^LL%d\n", header->cupsHeight);
                    puts("^MNN");
                }
                else if (ppdIsMarked(ppd_file, "zeMediaTracking", "Web"))
                {
                    puts("^MNY");
                }
                else if (ppdIsMarked(ppd_file, "zeMediaTracking", "Mark"))
                {
                    puts("^MNM");
                }

                if (header->cupsRowStep != 200)
                {
                    printf("^LT%d\n", header->cupsRowStep);
                }

                bool is_thermal = strcmp(header->MediaType, "Thermal") == 0;
                bool is_direct = strcmp(header->MediaType, "Direct") == 0;
                if (is_thermal)
                {
                    puts("^MTT");
                }
                else if (is_direct)
                {
                    puts("^MTD");
                }

                ppd_choice_t * choice_print_mode = ppdFindMarkedChoice(ppd_file, "zePrintMode");
                if (choice_print_mode)
                {
                    bool is_saved = strcmp(choice_print_mode->choice, "Saved") == 0;
                    bool is_tear = strcmp(choice_print_mode->choice, "Tear") == 0;
                    bool is_peel = strcmp(choice_print_mode->choice, "Peel") == 0;
                    bool is_rewind = strcmp(choice_print_mode->choice, "Rewind") == 0;
                    bool is_applicator = strcmp(choice_print_mode->choice, "Applicator") == 0;

                    if (!is_saved)
                    {
                        printf("^MM");
                        if (is_tear)
                        {
                            puts("T,Y");
                        }
                        else if (is_peel)
                        {
                            puts("P,Y");
                        }
                        else if (is_rewind)
                        {
                            puts("R,Y");
                        }
                        else if (is_applicator)
                        {
                            puts("A,Y");
                        }
                        else
                        {
                            puts("C,Y");
                        }
                    }
                }

                if (header->AdvanceDistance != 1000)
                {
                    if (header->AdvanceDistance < 0)
                    {
                        printf("~TA%04d\n", header->AdvanceDistance);
                    }
                    else
                    {
                        printf("~TA%03d\n", header->AdvanceDistance);
                    }
                }

                if (ppdIsMarked(ppd_file, "zeErrorReprint", "Always"))
                {
                    puts("^JZY");
                }
                else if (ppdIsMarked(ppd_file, "zeErrorReprint", "Never"))
                {
                    puts("^JZN");
                }

                if (header->NumCopies >= 2)
                {
                    printf("^PQ%d, 0, 0, N\n", header->NumCopies);
                }

                puts("^FO0,0^XGR:CUPS.GRF,1,1^FS");
                puts("^XZ");
                puts("^IDR:CUPS.GRF^FS");

                if (header->CutMedia != CUPS_CUT_NONE)
                {
                    puts("^CN1");
                }
            } // !global_is_cancelled
        } break;

        case 0x13:
        {
            if (header->AdvanceDistance != 1000)
            {
                printf("PRESENT_AT %d 1\r\n", header->AdvanceDistance);
            }

            if (ppdIsMarked(ppd_file, "zeErrorReprint", "Always"))
            {
                puts("ON-OUT-OF-PAPER WAIT\r");
            }
            else if (ppdIsMarked(ppd_file, "zeErrorReprint", "Never"))
            {
                puts("ON-OUT-OF-PAPER PURGE\r");
            }

            if (header->CutMedia != CUPS_CUT_NONE)
            {
                puts("CUT\r");
            }

            if (header->cupsCompression != 0)
            {
                printf("TONE %u\r\n",(unsigned int)(header->cupsCompression * 2));
            }

            ppd_choice_t * choice_print_rate = ppdFindMarkedChoice(ppd_file, "zePrintRate");
            if (choice_print_rate)
            {
                bool is_default = strcmp(choice_print_rate->choice, "Default") == 0;
                if (!is_default)
                {
                    printf("SPEED %d\r\n", atoi(choice_print_rate->choice));
                }
            }

            ppd_choice_t * choice_media_tracking = ppdFindMarkedChoice(ppd_file, "zeMediaTracking");
            if (choice_media_tracking)
            {
                bool is_continuous = strcmp(choice_media_tracking->choice, "Continuous");
                if (!is_continuous)
                {
                    puts("FORM\r");
                }
            }
            else
            {
                puts("FORM\r");
            }

            puts("PRINT\r");
        } break;

        case 0x14:
        {
            puts("PRINT 1,1");
        } break;

        case 0x20:
        {
            printf("\x1B""*rB");
            putchar(0xC);
        }

        default:
        {
            fputs("\x1E", stdout);
        } break;
    }

    fflush(stdout);

    free(global_buffer);
    global_buffer = nullptr;

    if (global_compress_buffer)
    {
        free(global_compress_buffer);
        global_compress_buffer = nullptr;
    }
    if (global_last_buffer)
    {
        free(global_last_buffer);
        global_last_buffer = nullptr;
    }
}

void
ZPLCompress(
    uint param_1,
    uint param_2)
{
    uint uVar1;

    if (1 < param_2) {
        if (399 < param_2) {
            do {
                putchar(0x7a);
                param_2 = param_2 - 400;
            } while (399 < param_2);
        }
        if (0x13 < param_2) {
            uVar1 = (param_2 & 0xffff) / 0x14;
            putchar(uVar1 + 0x66);
            param_2 = param_2 + uVar1 * -0x14 & 0xffff;
        }
        if (param_2 != 0) {
            putchar(param_2 + 0x46);
        }
    }
    putchar(param_1 & 0xff);
    return;
}

void
PCLCompress(
    char * buffer,
    unsigned int bytes_per_line)
{
    char * compress_buffer_current_position = (char *) global_compress_buffer;
    if (bytes_per_line)
    {
        char * current_position = buffer;
        char * buffer_end_position = buffer + bytes_per_line;
        char * last_buffer_position = (char *) global_last_buffer;

        while (current_position < buffer_end_position)
        {
            uint64_t starting_position_for_run = (uint64_t) current_position;

            uint64_t bytes_for_this_run;
            uint64_t offset;
            char * next_position;
            char * run_start_position;
            char control_byte;
            char current_buffer_byte_at_current_position;
            char last_buffer_byte_at_current_position;

            if (global_last_set == 0)
            {
                bytes_for_this_run = (uint64_t)(buffer_end_position - current_position);
                if (bytes_for_this_run > 8)
                {
                    bytes_for_this_run = 8;
                }

                next_position = current_position + bytes_for_this_run;
                run_start_position = current_position;

                // first 3 high bits hold the size for the compression
                control_byte = (bytes_for_this_run - 1) << 5;
            }
            else
            {
                current_buffer_byte_at_current_position = *current_position;
                last_buffer_byte_at_current_position = *last_buffer_position;

                while ((current_position < buffer_end_position) && (current_buffer_byte_at_current_position == last_buffer_byte_at_current_position))
                {
                    current_position++;
                    last_buffer_position++;
                    current_buffer_byte_at_current_position = *current_position;
                    last_buffer_byte_at_current_position = *last_buffer_position;
                }

                if (current_position == buffer_end_position)
                {
                    break;
                }

                offset = (uint64_t) current_position - starting_position_for_run;
                long mismatch_length = 0;

                // this is guaranteed to be true
                if (current_buffer_byte_at_current_position != last_buffer_byte_at_current_position)
                {
                    while (current_position + mismatch_length < buffer_end_position && mismatch_length < 8)
                    {
                        long index = mismatch_length + 1;
                        if (current_position[index] != last_buffer_position[index])
                        {
                            break;
                        }
                        mismatch_length++;
                    }

                    last_buffer_position += mismatch_length;
                }

                next_position = current_position + mismatch_length;

                bytes_for_this_run = mismatch_length;
                control_byte = (bytes_for_this_run - 1) << 5;
                if (offset < 0x1F)
                {
                    control_byte = control_byte | offset;
                    run_start_position = current_position;
                }
                else
                {
                    *compress_buffer_current_position = control_byte | 0x1F;
                    offset -= 0x1F;
                    compress_buffer_current_position++;

                    while (offset >= 0xFF)
                    {
                        *compress_buffer_current_position++ = 0xFF;
                        offset -= 0xFF;
                    }

                    control_byte = offset;
                    run_start_position = current_position;
                }
            }

            *compress_buffer_current_position++ = control_byte;
            memcpy(compress_buffer_current_position, run_start_position, bytes_for_this_run);

            current_position = next_position;
            compress_buffer_current_position += bytes_for_this_run;
        }
    }

    // Write the compressed stream
    int bytes_to_write = (int)((uint64_t)compress_buffer_current_position - (uint64_t) global_compress_buffer);
    printf("\x1B""&b%dW", bytes_to_write);
    fwrite(global_compress_buffer, bytes_to_write, 1, stdout);
    memcpy(global_last_buffer, buffer, bytes_per_line);
    global_last_set = 1;
}

void
OutputLine(
    cups_raster_t * raster_file,
    cups_page_header2_t * header,
    uint update_counter)
{
    switch (global_model_number)
    {
        case 0x10:
        {
            printf("\x1B""g%03d", header->cupsBytesPerLine);
            fwrite(global_buffer, 1, header->cupsBytesPerLine, stdout);
            fflush(stdout);
        } break;

        case 0x11:
        {
            // if buffer is null all the way, we're done
            if (*(char *) global_buffer == '\0')
            {
                int result = memcmp(global_buffer, (char *) global_buffer + 1, header->cupsBytesPerLine);
                if (result == 0)
                {
                    return;
                }
            }

            printf("GW0,%d,%d,1\n", update_counter, header->cupsBytesPerLine);
            if (header->cupsBytesPerLine)
            {
                unsigned int c = 0;
                char * p = (char *) global_buffer;
                do
                {
                    putchar(~p[c++]);
                } while (c != header->cupsBytesPerLine);
            }
            putchar(10);
            fflush(stdout);
        } break;

        case 0x12:
        {
            if (global_last_set)
            {
                int result = memcmp(global_buffer, global_last_buffer, header->cupsBytesPerLine);
                if (result == 0)
                {
                    putchar(0x3A);
                    return;
                }
            }

            char * compress_p = (char *) global_compress_buffer;
            char * p = (char *) global_buffer;
            if (header->cupsBytesPerLine)
            {
                int c = 0;
                do
                {
                    compress_p[c * 2] = "0123456789ABCDEF"[p[c] >> 4];
                    compress_p[c * 2 + 1] = "0123456789ABCDEF"[p[c] & 0xF];
                    c++;
                } while (c != header->cupsBytesPerLine);
                compress_p += 2 * c;
            }
            *compress_p = '\0';
            char c1 = *(char *)global_compress_buffer;
            char c2 = ((char *)global_compress_buffer)[1];
            unsigned int var_compress;
            if (c2)
            {
                char * c_p = (char *) global_compress_buffer + 2;
                var_compress = 1;
                do
                {
                    if (c1 == c2)
                    {
                        var_compress++;
                    }
                    else
                    {
                        ZPLCompress(c1, var_compress);
                        c2 = *(c_p - 1);
                        var_compress = 1;
                    }

                    c1 = c2;
                    c2 = *c_p++;
                } while (c2 != '\0');
            }
            else
            {
                var_compress = 1;
            }

            if (c1 == '0')
            {
                if ((var_compress & 1) != 0)
                {
                    var_compress--;
                    putchar(0x30);
                }
                if (var_compress != 0)
                {
                    putchar(0x2C);
                }
            }
            else
            {
                ZPLCompress(c1, var_compress);
            }

            fflush(stdout);
            memcpy(global_last_buffer, global_buffer, header->cupsBytesPerLine);
            global_last_set = 1;
        } break;

        case 0x13:
        {
            if (* (char *) global_buffer == '\0')
            {
                if (!memcpy(global_buffer, (char *) global_buffer + 1, header->cupsBytesPerLine))
                {
                    return;
                }
            }
            printf("CG %u 1 0 %d ", header->cupsBytesPerLine, (unsigned int) update_counter);
            fwrite(global_buffer, 1ull, header->cupsBytesPerLine, stdout);
            puts("\r");
            fflush(stdout);
        } break;

        case 0x14:
        {
            unsigned long bytes_per_line = (header->cupsWidth + 7) / 8;
            if (bytes_per_line > 0)
            {
                char *current_position = (char *)global_buffer;

                for (unsigned long byte = 0; byte < bytes_per_line; byte++)
                {
                    unsigned char c = 0;
                    for (int bit_index = 0; bit_index < 8; bit_index++)
                    {
                        int pixel_index = byte * 8 + bit_index;

                        if (pixel_index < header->cupsWidth)
                        {
                            unsigned char value = current_position[pixel_index];
                            if (value <= (unsigned char) 0x80)
                            {
                                c |= (1 << (7 - bit_index));
                            }
                        }
                    }
                    putchar(~c);
                }
            }

            fflush(stdout);
        } break;

        case 0x20:
        {
            char * c_p = (char *) global_buffer;
            if (*c_p || (memcmp(global_buffer, (char *) global_buffer + 1, header->cupsBytesPerLine - 1) != 0))
            {
                if (global_feed)
                {
                    printf("\x1B""*b%dY", global_feed);
                    global_feed = 0;
                    global_last_set = 0;
                }

                PCLCompress(c_p, header->cupsBytesPerLine);
                return;
            }
            global_feed++;
        } break;

        default:
        {
            if (*(char *)global_buffer == '\0')
            {
                if (memcmp(global_buffer, (char *) global_buffer + 1, header->cupsBytesPerLine - 1) != 0)
                {
                    global_feed++;
                    return;
                }
            }

            if (global_feed)
            {
                if (global_feed >= 0x100)
                {
                    do
                    {
                        printf("\x1B""f""\x01""%c", 255);
                        global_feed -= 255;
                    } while (global_feed > 0xFF);
                }

                printf("\x1B""f""\x01""%c", global_feed);
                global_feed = 0;
            }

            putchar(22);
            fwrite(global_buffer, 1, header->cupsBytesPerLine, stdout);
            fflush(stdout);
        } break;
    }
}

#include <unistd.h>
#include <pwd.h>
int
main(int argc, char * argv[])
{
    // turn off stderr buffering
    setvbuf(stderr, nullptr, _IONBF, 0);
    int raster_file_descriptor = 0;

    uid_t uid = getuid();
    struct passwd * pw = getpwuid(uid);
    log(std::format("User: {}", pw->pw_name));
    log(std::format("argc: {}", argc));

    for (int i = 0; i < argc; i++)
    {
        log(std::format("argv[{}]: {}", i, argv[i]));
    }

    // validate input
    if (argc != 6 && argc != 7)
    {
        log("Invalid arg count");
        message_scheduler(SCHEDULER_ERROR, "Invalid arg count");
        return 1;
    }

    // First run
    if (argc == 7)
    {
        log("Opening raster file");
        raster_file_descriptor = open(argv[6], O_RDONLY);
        log(std::format("Raster file descriptor: {}", (uint64_t) raster_file_descriptor));

        if (raster_file_descriptor == -1)
        {
            log("Unable to open raster file");
            message_scheduler(SCHEDULER_ERROR, "Unable to open raster file");
            return 1;
        }
    }

    cups_raster_t * raster_file = cupsRasterOpen(raster_file_descriptor, CUPS_RASTER_READ);
    log(std::format("Raster file: {}", (uint64_t) raster_file));
    if (!raster_file)
    {
        log("Raster File Open failed");
        message_scheduler(SCHEDULER_ERROR, "Raster File Open Failed");
        // Skipping DEBUG log for what the error was
        return 1;
    }

    global_is_cancelled = 0;

    sigset(SIGTERM, signal_handler);
    cups_option_t * options;
    int num_options = cupsParseOptions(argv[5], 0, &options);
    ppd_file_t * ppd_file = ppdOpenFile(getenv("PPD"));

    if (!ppd_file)
    {
        log("PPD file could not be opened");
        message_scheduler(SCHEDULER_ERROR, "PPD file could not be opened");
        // Skipping DEBUG log for what the error was
        return 1;
    }

    ppdMarkDefaults(ppd_file);
    cupsMarkOptions(ppd_file, num_options, options);

    Setup(ppd_file);
    log("Setup complete");
    log(std::format("model: {}", global_model_number));
    global_page = 0;

    cups_page_header2_t header = {};

    do {
        // Check if cancelled after reading header
        log("RasterRead");
        if (!cupsRasterReadHeader2(raster_file, &header) || global_is_cancelled) break;
        log("RasterRead done");

        global_page++;
        message_scheduler(SCHEDULER_PAGE, std::format("{} 1", global_page));
        message_scheduler(SCHEDULER_INFO, std::format("Starting page {}", global_page));

        log("calling StartPage");
        StartPage(ppd_file, &header);
        log("StartPage complete");

        // One last check before printing (after start page)
        if (global_is_cancelled) break;

        if (!header.cupsHeight || !header.cupsBytesPerLine) continue;

        uint line_count = 0;
        uint update_counter = 0;
        while (true)
        {
            // Update status every 16 loops
            if ((update_counter & 0xF) == 0)
            {
                ulong progress = line_count / (float) header.cupsHeight;
                message_scheduler(SCHEDULER_INFO, std::format("Printing page {}, {}% complete.", global_page, progress));
                message_scheduler(SCHEDULER_ATTR, std::format("job-media-progress={}", progress));
                update_counter = 0;
            }

            if (!cupsRasterReadPixels(raster_file, (unsigned char *)global_buffer, header.cupsBytesPerLine)) break;

            OutputLine(raster_file, &header, update_counter);
            update_counter++;

            if (update_counter >= header.cupsHeight) break;
            line_count += 100;
        }

        EndPage(ppd_file, &header);
        log("EndPage complete");
        message_scheduler(SCHEDULER_INFO, std::format("Finished page {}", global_page));
    } while (!global_is_cancelled);

    log("wrapping up");
    cupsRasterClose(raster_file);
    if (raster_file_descriptor) close(raster_file_descriptor);
    raster_file = nullptr;

    ppdClose(ppd_file);
    cupsFreeOptions(num_options, options);

    if (global_page) return 0;
    return 1;
}
