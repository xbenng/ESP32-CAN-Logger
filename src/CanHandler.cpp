#include <Arduino.h>
#include "CanHandler.h"
#include "ESP32CAN.h"
#include "CAN_config.h"
#include "SD_MMC.h"
#include "WROVER_KIT_LCD.h"
#include "Display.h"

CAN_device_t CAN_cfg;               // CAN Config
extern WROVER_KIT_LCD tft;

namespace CAN
{

    
    class LineDisplay
    {
    public:

        class ILine
        {
        public:
            virtual uint32_t draw(WROVER_KIT_LCD& display);  // draws and returns how many pixel rows used
        };

        LineDisplay(int x_fixed, int y_fixed) :
            x_fixed{x_fixed},
            y_fixed{y_fixed}
        {}

        void register_line(ILine& line)
        {
            line_list[line_list_count++] = &line;
        }

        void draw_all()
        {            
            SemaphoreHandle_t& display_lock { Display::get_lock() };
            if (display_lock != NULL)
            {
                if (xSemaphoreTake( display_lock, ( TickType_t ) 10 ) == pdTRUE)
                {
                    int y_current = y_fixed;
                    for (int i = 0; i < line_list_count; i++)
                    {
                        tft.setCursor(x_fixed, y_current);
                        y_current += line_list[i]->draw(tft);
                    }
                    xSemaphoreGive( display_lock );
                }
            }
        }
    private:
        int x_fixed;
        int y_fixed;
        ILine* line_list[50];
        int line_list_count;
    };

    static LineDisplay display {0,50};

    class CanMsgLine : public LineDisplay::ILine
    {
    public:


        CanMsgLine()
        {}
        
        uint32_t draw(WROVER_KIT_LCD& display) override
        {
            constexpr int line_height {20};

            int x_original {display.getCursorX()};
            int y_original {display.getCursorY()};
            display.fillRect(x_original, y_original, display.width(), line_height, WROVER_BLACK);
            display.printf("ID: 0x%03X  Count: %d\n", id, count);
            for (int i = 0; i < last_dlc; i++) {
                display.printf("0x%02X ", last_data[i]);
            }
            return line_height;
        }

        bool process_message(CAN_frame_t& frame)
        {
            bool assigned_id {false};
            if (id == -1)
            {
                id = frame.MsgID;
                assigned_id = true;
            }

            count++;
            last_dlc = frame.FIR.B.DLC;
            memcpy(last_data, frame.data.u8, last_dlc);
            return assigned_id;
        }

        int get_id()
        {
            return id;
        }

    private:
        int count {0};
        int id {-1};
        uint8_t last_dlc;
        uint8_t last_data[8];


    };

    CanMsgLine msg_list[50];
    int msg_list_cnt {0};
    void add_message(CAN_frame_t& frame)
    {
        bool id_exists {false};
        for (int i = 0; i < msg_list_cnt; i++)
        {
            if (msg_list[i].get_id() == frame.MsgID)
            {
                msg_list[i].process_message(frame);

                id_exists = true;
                break;
            }
        }

        if (!id_exists)
        {
            display.register_line(msg_list[msg_list_cnt]);
            msg_list[msg_list_cnt++].process_message(frame);
        }
    }

    class CanLog
    {
    public:
        CanLog(FS& fs, const char* rootdir) :
            fs{fs},
            rootdir{rootdir}
        {

        }

        void start()
        {
            if (!started)
            {
                // make directory if it doesn't exist
                if (!fs.exists(rootdir))
                {
                    fs.mkdir(rootdir);
                }

                uint32_t max_log_num { 0 };
                File root = fs.open(rootdir);
                for (File file = root.openNextFile(); file; file = root.openNextFile())  // search each existing file.
                {
                    if(file.isDirectory()){
                        Serial.print("  DIR : ");
                        Serial.println(file.name());
                    } else {
                        Serial.print("  FILE: ");
                        Serial.print(file.name());
                        Serial.print("  SIZE: ");
                        Serial.println(file.size());
                    }

                    String filename {file.name()};
                    filename.remove(0,String(rootdir).length()+1);  // trim directory off front
                    if (filename.startsWith(log_prefix))
                    {
                        int log_num {filename.substring(String(log_prefix).length(), filename.indexOf('.')).toInt()};

                        if (log_num > max_log_num)
                        {
                            max_log_num = log_num;
                        }
                    }
                }

                start_time_ms = millis();
                String new_file_name {String(rootdir) + "/"+ log_prefix + String(max_log_num + 1) + ".csv"};
                logfile = fs.open(new_file_name, FILE_WRITE);
                tft.println("created file: " + new_file_name);
                started = true;
            }
        }

        void stop()
        {
            logfile.close();
            started = false;
        }

        void log_message(CAN_frame_t& frame)
        {
            if (started && logfile)
            {
                
                constexpr int max_line_length { 32+5+(4*8) };
                char line_buffer[max_line_length];
                char* line_buffer_ptr = line_buffer;
                // log
                line_buffer_ptr += sprintf(line_buffer_ptr, "%.3f,", (frame.time_us / 1000.0f) - start_time_ms);
                line_buffer_ptr += sprintf(line_buffer_ptr, "0x%03X,", frame.MsgID);
                for (int i = 0; i < frame.FIR.B.DLC; i++) {
                    line_buffer_ptr += sprintf(line_buffer_ptr, "0x%02X,", frame.data.u8[i]);
                }
                log_flush_count += logfile.println(line_buffer);

                constexpr int flush_threshold {max_line_length * 20};
                if (log_flush_count > flush_threshold)
                {
                    // logfile.flush();
                    log_flush_count = 0;
                }

                add_message(frame);
            }
        }

    private:
        static constexpr const char* log_prefix{"log_"};

        unsigned long start_time_ms;
        File logfile;
        FS fs;
        bool started { false };
        const char* rootdir;
        int log_flush_count {0};


    };
    static CanLog log { SD_MMC, "/logs" };

    void start_log()
    {
        log.start();
    }
    
    void stop_log()
    {
        log.stop();
    }

    static void rx_can(void * pvParameters)
    {
        CAN_frame_t rx_frame;

        for (;;)
        {
            // Receive next CAN frame from queue
            if (xQueueReceive(CAN_cfg.rx_queue, &rx_frame, 4 * portTICK_PERIOD_MS) == pdTRUE) 
            {
                log.log_message(rx_frame);
            }
        }
    }

    static void update_display(void * pvParameters)
    {
        TickType_t xLastWakeTime;
        const TickType_t xFrequency = portTICK_RATE_MS * 100;

        // Initialise the xLastWakeTime variable with the current time.
        xLastWakeTime = xTaskGetTickCount();

        for( ;; )
        {
            // Wait for the next cycle.
            vTaskDelayUntil( &xLastWakeTime, xFrequency );
            // Perform action here.
            display.draw_all();
        }
    }

    void setup()
    {
        constexpr int rx_queue_size = 25;       // Receive Queue size

        //setup can
        CAN_cfg.speed = CAN_SPEED_1000KBPS;
        CAN_cfg.tx_pin_id = GPIO_NUM_26;
        CAN_cfg.rx_pin_id = GPIO_NUM_27;
        CAN_cfg.rx_queue = xQueueCreate(rx_queue_size, sizeof(CAN_frame_t));
        
        // Init CAN Module
        ESP32Can.CANInit();

        xTaskCreate(rx_can, "rx_can", 4096, 0, 2, NULL);
        xTaskCreatePinnedToCore(update_display, "update_display", 2048, 0, 10, NULL, 0);
    }

}