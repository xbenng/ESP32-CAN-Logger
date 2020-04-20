#include <Arduino.h>
#include "CanHandler.h"
#include "ESP32CAN.h"
#include "CAN_config.h"
#include "SD_MMC.h"
#include "WROVER_KIT_LCD.h"

CAN_device_t CAN_cfg;               // CAN Config
extern WROVER_KIT_LCD tft;

namespace CAN
{

    
    class Display
    {
    public:

        class ILine
        {
        public:
            virtual uint32_t update();
        };

        Display(int x_fixed, int y_fixed) :
            x_fixed{x_fixed},
            y_fixed{y_fixed}
        {}

        void register_line(ILine line)
        {
            line_list[line_list_count++] = line;
        }

        void update()
        {
            for (int i = 0; i < line_list_count; i++)
            {
                line_list[i].update();
            }
        }
    private:
        int x_fixed;
        int y_fixed;
        ILine line_list[50];
        int line_list_count;
    };

    class CanMsgDisplayLine : public Display::ILine
    {
    public:


        CanMsgDisplayLine()
        {}
        
        uint32_t update() override
        {
            return 0;
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
            return assigned_id;
        }

        int get_id()
        {
            return id;
        }

    private:
        int count {0};
        int id {-1};


    };

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
            if (!fs.exists(rootdir))
            {
                fs.mkdir(rootdir);
            }

            uint32_t num_files { 0 };
            File root = fs.open(rootdir);
            for (File file = root.openNextFile(); file; file = root.openNextFile())
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
                    num_files++;
            }

            start_time_ms = millis();
            String new_file_name {String(rootdir) + "/"+ log_prefix + String(num_files) + ".csv"};
            logfile = fs.open(new_file_name, FILE_WRITE);
            tft.println("created file: " + new_file_name);
            started = true;
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
                if (frame.FIR.B.FF == CAN_frame_std) {
                Serial.printf("New standard frame");
                }
                else {
                Serial.printf("New extended frame");
                }

                if (frame.FIR.B.RTR == CAN_RTR) {
                Serial.printf(" RTR from 0x%08X, DLC %d\r\n", frame.MsgID,  frame.FIR.B.DLC);
                }
                else {
                Serial.printf(" from 0x%08X, DLC %d, Data ", frame.MsgID,  frame.FIR.B.DLC);
                for (int i = 0; i < frame.FIR.B.DLC; i++) {
                    Serial.printf("0x%02X ", frame.data.u8[i]);
                }
                Serial.printf("\n");
                }
                
                constexpr int max_line_length { 32+5+(4*8) };
                char line_buffer[max_line_length];
                char* line_buffer_ptr = line_buffer;
                // log
                line_buffer_ptr += sprintf(line_buffer_ptr, "%d,", millis() - start_time_ms );
                line_buffer_ptr += sprintf(line_buffer_ptr, "0x%03X,", frame.MsgID);
                for (int i = 0; i < frame.FIR.B.DLC; i++) {
                    line_buffer_ptr += sprintf(line_buffer_ptr, "0x%02X,", frame.data.u8[i]);
                }
                logfile.println(line_buffer);
                // tft.println(line_buffer);
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

        CanMsgDisplayLine msg_list[50];
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
                msg_list[msg_list_cnt++].process_message(frame);
            }
        }
    };
    static CanLog log { SD_MMC, "/logs" };

    static unsigned long previousMillis = 0;   // will store last time a CAN Message was send
    static const int interval = 1000;          // interval at which send CAN Messages (milliseconds)
    static const int rx_queue_size = 10;       // Receive Queue size

    duk_ret_t start_log(duk_context *ctx)
    {
        log.start();
        return 0;
    }
    
    duk_ret_t stop_log(duk_context *ctx)
    {
        log.stop();
        return 0;
    }

    static void rx_can(void * pvParameters)
    {
        CAN_frame_t rx_frame;

        for (;;)
        {
            unsigned long currentMillis = millis();

            // Receive next CAN frame from queue
            if (xQueueReceive(CAN_cfg.rx_queue, &rx_frame, 1 * portTICK_PERIOD_MS) == pdTRUE) 
            {
                log.log_message(rx_frame);
            }
        }
    }

    void setup()
    {
        //setup can
        CAN_cfg.speed = CAN_SPEED_125KBPS;
        CAN_cfg.tx_pin_id = GPIO_NUM_26;
        CAN_cfg.rx_pin_id = GPIO_NUM_27;
        CAN_cfg.rx_queue = xQueueCreate(rx_queue_size, sizeof(CAN_frame_t));
        
        // Init CAN Module
        ESP32Can.CANInit();

        xTaskCreate(rx_can, "rx_can", 2048, 0, 2, NULL);
    }

}