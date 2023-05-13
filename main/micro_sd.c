#include "main.h"
#include "micro_sd.h"

#define MOUNT_POINT "/sdcard"
#define SPI_DMA_CHAN 2
const char *SD = "SD_TASK";

xQueueHandle sd_evt_queue;


uint8_t sd_onoff=0;       //存储开关
uint8_t sd_state = 0;	  // sd卡是否已经挂载
uint8_t time_state = 0;	  // RTC是否正常
uint32_t bytes_total = 0; //全部容量 
uint32_t bytes_free = 0;  //剩余容量

time_t now;
struct tm timeinfo;

void example_get_fatfs_usage(size_t *out_total_bytes, size_t *out_free_bytes)
{
	FATFS *fs;
	size_t free_clusters;
	int res = f_getfree("0:", &free_clusters, &fs);
	assert(res == FR_OK);
	size_t total_sectors = (fs->n_fatent - 2) * fs->csize;
	size_t free_sectors = free_clusters * fs->csize;

	// assuming the total size is < 4GiB, should be true for SPI Flash
	if (out_total_bytes != NULL)
	{
		*out_total_bytes = total_sectors * fs->ssize;
	}
	if (out_free_bytes != NULL)
	{
		*out_free_bytes = free_sectors * fs->ssize;
	}
}

//设置ESP32系统日期时间
void setTime(int sc, int mn, int hr, int dy, int mt, int yr)
{
	// seconds, minute, hour, day, month, year $ microseconds(optional)
	// ie setTime(20, 34, 8, 1, 4, 2021) = 8:34:20 1/4/2021
	struct tm t = {0};	   // Initalize to all 0's
	t.tm_year = yr - 1900; // This is year-1900, so 121 = 2021
	t.tm_mon = mt - 1;
	t.tm_mday = dy;
	t.tm_hour = hr;
	t.tm_min = mn;
	t.tm_sec = sc;
	time_t timeSinceEpoch = mktime(&t);
	//   setTime(timeSinceEpoch, ms);
	struct timeval now = {.tv_sec = timeSinceEpoch};
	settimeofday(&now, NULL);
}

uint64_t dianya = 0;


void micro_sd_task(void *arg)
{

	esp_err_t ret;							// ESP错误定义
	sdmmc_card_t *card;						// SD / MMC卡信息结构
	const char mount_point[] = MOUNT_POINT; // 根目录
	char ReadFileBuff[64];
	char strftime_buf[64];
	time_t now;
	struct tm timeinfo;
	uint8_t evt;
	FILE *f;
	uint8_t not = 0;
	uint8_t hour = 0;
	uint8_t second = 0;
	char sign_ch = '-';

	const char ch_gbk1[] = {0xC9, 0xE8, 0xB1, 0xB8, 0x49, 0x44, 0x3A, 0x00};																			   //设备ID:
	const char ch_gbk2[] = {0xC8, 0xD5, 0xC6, 0xDA, 0xCA, 0xB1, 0xBC, 0xE4, 0x2C, 0xB2, 0xE2, 0xC1, 0xBF, 0xD6, 0xB5, 0x2C, 0xB5, 0xA5, 0xCE, 0xBB, 0x00}; //日期时间,测量值,单位
	const char ch_gbk3[] = {0xC8,0xD5,0xC6,0xDA,0xCA,0xB1,0xBC,0xE4,0x2C,0xB5,0xE7,0xD1,0xB9,0x28,0x6D,0x56,0x29,0x2C,0xB5,0xE7,0xC1,0xF7,0x28,0x6D,0x41,0x29,0x2C,0xB9,0xA6,0xC2,0xCA,0x28,0x6D,0x57,0x29,0x00}; //日期时间,电压(mV),电流(mA),功率(mW)
	const char ch_gbk4[] = {0xC8,0xD5,0xC6,0xDA,0xCA,0xB1,0xBC,0xE4,0x2C,0xB5,0xE7,0xD7,0xE8,0xD6,0xB5,0x28,0xA6,0xB8,0x29,0x2C,0xC9,0xE3,0xCA,0xCF,0xB6,0xC8,0x2C,0xBB,0xAA,0xCA,0xCF,0xB6,0xC8,0x00};//日期时间,电阻值(Ω),摄氏度,华氏度
	const char ch_gbk5[] = {0xC8,0xD5,0xC6,0xDA,0xCA,0xB1,0xBC,0xE4,0x2C,0xD6,0xA1,0xC0,0xE0,0xD0,0xCD,0x2C,0xD6,0xA1,0xB8,0xF1,0xCA,0xBD,0x2C,0xD6,0xA1,0x49,0x44,0x2C,0xCA,0xFD,0xBE,0xDD,0x00}; //日期时间,帧类型,帧格式,帧ID,数据
	const char ch_gbk6[] = {0xB1,0xEA,0xD7,0xBC,0xD6,0xA1,0x00};//标准帧
	const char ch_gbk7[] = {0xC0,0xA9,0xD5,0xB9,0xD6,0xA1,0x00};//扩展帧
	const char ch_gbk8[] = {0xCA,0xFD,0xBE,0xDD,0xD6,0xA1,0x00};//数据帧
	const char ch_gbk9[] = {0xD4,0xB6,0xB3,0xCC,0xD6,0xA1,0x00};//远程帧

	sd_evt_queue = xQueueCreate(3, sizeof(uint8_t));

	esp_vfs_fat_sdmmc_mount_config_t mount_config = {								  // 文件系统挂载配置
													 .format_if_mount_failed = false, // 如果挂载失败：true会重新分区和格式化/false不会重新分区和格式化
													 .max_files = 1,				  // 打开文件最大数量
													 .allocation_unit_size = 2 * 1024};

	sdmmc_host_t host = SDSPI_HOST_DEFAULT();
	host.slot = SPI3_HOST; //修改总线
	spi_bus_config_t bus_cfg = {
		.mosi_io_num = PIN_NUM_MOSI,
		.miso_io_num = PIN_NUM_MISO,
		.sclk_io_num = PIN_NUM_CLK,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 4000,
	};
	// SPI总线初始化
	ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
	if (ret != ESP_OK)
	{
		//ESP_LOGI(SD, "Failed to initialize bus.\r\n");
		return;
	}

	// 这将初始化没有卡检测（CD）和写保护（WP）信号的插槽。
	// 如果您的主板有这些信号，请修改slot_config.gpio_cd和slot_config.gpio_wp。
	sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
	slot_config.gpio_cs = PIN_NUM_CS;
	slot_config.host_id = host.slot;
	// 挂载文件系统
	ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
	if (ret != ESP_OK)
	{
		sd_state = 0;
	}
	else
	{
		sd_state = 1;
		// TF卡已经初始化，打印TF卡属性
		sdmmc_card_print_info(stdout, card);
		// Print FAT FS size information
		example_get_fatfs_usage(&bytes_total, &bytes_free);
		
	//	ESP_LOGI(SD,"%s->FAT FS Total: %d MB, Free: %d MB \r\n", SD, bytes_total /1024/1024, bytes_free /1024/1024);
	}

	// 使用POSIX和C标准库函数来处理文件。
	// printf("%s->Opening file\r\n",SD);
	// FILE* f = fopen(MOUNT_POINT"/hello.txt", "w");// 创建一个文件
	// if (f == NULL) {
	// 	printf("%s->Failed to open file for writing\r\n",SD);
	// 	return;
	// }
	// fprintf(f, "Hello %s!\n", card->cid.name);
	// fclose(f);
	// printf("%s->File written\r\n",SD);

	// 使用POSIX和C标准库函数来处理文件。
	// printf("%s->Opening file\r\n",SD);
	// FILE* f = fopen(MOUNT_POINT"/data.csv", "w");// 创建一个文件
	// if (f == NULL) {
	// 	printf("%s->Failed to open file for writing\r\n",SD);
	// 	return;
	// }
	// fprintf(f, "设备ID,档位,测量值,单位\n");
	// fprintf(f, "CU12345678,电压档,123.45,V\n");
	// fprintf(f, "CU12345678,电压档,123.46,V\n");
	// fprintf(f, "CU12345678,电压档,123.47,V\n");
	// fprintf(f, "CU12345678,电压档,123.48,V\n");
	// fclose(f);
	// printf("%s->File written\r\n",SD);

	/*
	   // 重命名前检查目标文件是否存在
	   struct stat st;
	   if (stat(MOUNT_POINT"/foo.txt", &st) == 0) {
		   unlink(MOUNT_POINT"/foo.txt");// 删除（如果存在）
	   }

	   // 重命名文件
	   printf("%s->Renaming file\r\n",SD);
	   if (rename(MOUNT_POINT"/hello.txt", MOUNT_POINT"/foo.txt") != 0) {
		   printf("%s->Rename failed\r\n",SD);
		   return;
	   }

	   // 读取文件
	   printf("%s->Reading file\r\n",SD);
	   f = fopen(MOUNT_POINT"/foo2.txt", "r");// 读取方式打开文件
	   if (f == NULL) {
		   printf("%s->Failed to open file for reading\r\n",SD);
		   return;
	   }
	   fgets(ReadFileBuff, sizeof(ReadFileBuff), f);	// 读取一行数据
	   fclose(f);										// 关闭文件
	   char* pos = strchr(ReadFileBuff, '\n');			// 在字符串中查找换行
	   if (pos) {
		   *pos = '\0';								// 替换为结束符
	   }
	   printf("%s->Read from file: '%s'\r\n",SD,ReadFileBuff);
	*/
	// // 卸载分区并禁用SDMMC或SPI外设
	// esp_vfs_fat_sdcard_unmount(mount_point, card);
	// printf("%s->Card unmounted\r\n",SD);
	// //卸载总线
	// spi_bus_free(host.slot);

	setenv("TZ", "CST-8", 1);
	tzset();

	struct stat st;
	vTaskDelay(pdMS_TO_TICKS(1000));
	PCF8563_Init();
	vTaskDelay(pdMS_TO_TICKS(1000));
	PCF8563_ReadTimes();
	ESP_LOG_BUFFER_HEX(SD, PCF8563_Time, 7);
	ESP_LOGE(SD, "VL_value=%d", VL_value);
	if (VL_value != 0) //时钟芯片未校时
	{
		time_state = 0;
	}
	else //时钟芯片已经校时，开始对系统时间进行校准
	{
		time_state = 1;
		setTime(PCF8563_Time[0], PCF8563_Time[1], PCF8563_Time[2], PCF8563_Time[3], PCF8563_Time[5], 2000 + PCF8563_Time[6]);
	}

	char file_name[50];
	char date_hour[11] = "0000000000\0";
	xQueueReceive(sd_evt_queue, &evt, 1000);
	xQueueReceive(sd_evt_queue, &evt, 1000);
	xQueueReceive(sd_evt_queue, &evt, 1000);
	xQueueReceive(sd_evt_queue, &evt, 1000);
	xQueueReceive(sd_evt_queue, &evt, 1000);
	xQueueReceive(sd_evt_queue, &evt, 1000);
	while (1)
	{
		// vTaskDelay(pdMS_TO_TICKS(1000));

		if (xQueueReceive(sd_evt_queue, &evt, portMAX_DELAY))
		{
			if ((time_state != 0) & (sd_state != 0)& (sd_onoff != 0)) //校时成功并且tf卡可用 才可以操作TF卡
			{
				not = 0;
				switch (evt)
				{
				case SD_SWFUN: //切换档位，需要重新打开文件或者新建文件


					time(&now);
					localtime_r(&now, &timeinfo);

					date_hour[0] = (timeinfo.tm_year + 1900) / 1000 % 10 + '0';
					date_hour[1] = (timeinfo.tm_year + 1900) / 100 % 10 + '0';
					date_hour[2] = (timeinfo.tm_year + 1900) / 10 % 10 + '0';
					date_hour[3] = (timeinfo.tm_year + 1900) % 10 + '0';

					date_hour[4] = (timeinfo.tm_mon + 1) / 10 % 10 + '0';
					date_hour[5] = (timeinfo.tm_mon + 1) % 10 + '0';

					date_hour[6] = (timeinfo.tm_mday) / 10 % 10 + '0';
					date_hour[7] = (timeinfo.tm_mday) % 10 + '0';

					date_hour[8] = (timeinfo.tm_hour) / 10 % 10 + '0';
					date_hour[9] = (timeinfo.tm_hour) % 10 + '0';

					switch (current_fun)
					{
					case FUN_DCV:
						strcpy(file_name, "/sdcard/直流电压-");
						break;
					case FUN_ACV:
						strcpy(file_name, "/sdcard/交流电压-");
						break;
					case FUN_DCA:
						strcpy(file_name, "/sdcard/直流电流-");
						break;
					case FUN_ACA:
						strcpy(file_name, "/sdcard/交流电流-");
						break;
					case FUN_2WR:
						strcpy(file_name, "/sdcard/两线电阻-");
						break;
					case FUN_4WR:
						strcpy(file_name, "/sdcard/四线低阻-");
						break;
					case FUN_DIODE:
						strcpy(file_name, "/sdcard/二极管-");
						break;
					case FUN_BEEP:
						strcpy(file_name, "/sdcard/通断档-");
						break;
					case FUN_DCW:
						strcpy(file_name, "/sdcard/直流功率-");
						break;
					case FUN_ACW:
						strcpy(file_name, "/sdcard/交流功率-");
						break;
					case FUN_TEMP:
						strcpy(file_name, "/sdcard/测量温度-");
						break;
					case FUN_UART:
						strcpy(file_name, "/sdcard/UART-");
						break;
					case FUN_RS485:
						strcpy(file_name, "/sdcard/RS485-");
						break;
					case FUN_CAN:
						strcpy(file_name, "/sdcard/CAN-");
						break;

					default:
						not = 1;
						break;
					}

					if (not == 0)
					{

						strcat(file_name, date_hour);
						if((current_fun==FUN_UART)|(current_fun==FUN_RS485)) //串口数据用txt存储
						{
							strcat(file_name, ".txt");
						}
						else
						{
							strcat(file_name, ".csv");
						}
						
						// printf(MOUNT_POINT"/FUN_DCV.csv");
						struct stat buf;
						int zt = stat(file_name, &buf); // zt为-1说明没有这个文件

						// if (zt == 0) printf("file size = %ld \n", buf.st_size);

						f = fopen(file_name, "a");
						if (f == NULL)
						{
							sd_state = 0;
							evt=REFRESH_LEFT_ICON;
                  			xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
						}
						else
						{

							if (zt == -1)																																		   
							{//创建新文件后，先写入文件头
								
								if((current_fun==FUN_DCW)|(current_fun==FUN_ACW)) //功率
								{
									fprintf(f, "%s,%s\n", ch_gbk1, device_ID);
									fprintf(f, "%s\n", ch_gbk3);
								}
								else if(current_fun==FUN_TEMP)//温度
								{
									fprintf(f, "%s,%s\n", ch_gbk1, device_ID);
									fprintf(f, "%s\n", ch_gbk4);
								}
								else if((current_fun==FUN_UART)|(current_fun==FUN_RS485))//串口
								{
									
								}
								else if(current_fun==FUN_CAN)//CAN
								{
									fprintf(f, "%s,%s\n", ch_gbk1, device_ID);
									fprintf(f, "%s\n", ch_gbk5);
								}
								else
								{
									fprintf(f, "%s,%s\n", ch_gbk1, device_ID);
									fprintf(f, "%s\n", ch_gbk2);
								}	
							}
							if (fclose(f) == -1)
							{
								sd_state = 0;
								evt=REFRESH_LEFT_ICON;
                  				xQueueSendFromISR(gui_evt_queue, &evt, NULL); 		
							}
						//	ESP_LOGI(SD, "SD write1111");
						}
					}

					break;

				case SD_RECEIVE_ADC: //收到ADC测量值

					if (hour != ADC_timeinfo.tm_hour) //如果小时变了，新建一个文件存储
					{
						hour = ADC_timeinfo.tm_hour;
						evt = SD_SWFUN;
						xQueueSendFromISR(sd_evt_queue, &evt, NULL);
					}
					else
					{

						f = fopen(file_name, "a");
						if (f == NULL)
						{
							sd_state = 0;
							evt=REFRESH_LEFT_ICON;
                  			xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
						}
						else
						{
							strftime(strftime_buf, sizeof(strftime_buf), "%Y/%m/%d %X", &ADC_timeinfo);
							//printf("time = %s \n", strftime_buf);

							switch (current_fun)
							{
							case FUN_DCV:
							case FUN_ACV:
								sign_ch = sign ? '-' : 0x00; //正负号
								if (unit == 0x01)				 // uV
								{
									fprintf(f, "%s,%c%.2f,%s\n", strftime_buf, sign_ch, (float)measured_value / 1000.0, "mV");
								}
								else if (unit == 0x02) // mV
								{
									fprintf(f, "%s,%c%u,%s\n", strftime_buf, sign_ch, measured_value, "mV");
								}
								break;
							case FUN_DCA:
							case FUN_ACA:
								sign_ch = sign ? '-' : 0x00; //正负号
								if (unit == 0x04)				 // nA
								{
									fprintf(f, "%s,%c%.2f,%s\n", strftime_buf, sign_ch, (float)measured_value / 1000.0, "uA");
								}
								else if (unit == 0x05) // uA
								{
									if (measured_value < 1000000) //小于1A
									{
										fprintf(f, "%s,%c%.2f,%s\n", strftime_buf, sign_ch, (float)measured_value / 1000.0, "mA");
									}
									else
									{
										fprintf(f, "%s,%c%.2f,%s\n", strftime_buf, sign_ch, (float)measured_value / 1000000.0, "A");
									}
								}
								break;
							case FUN_2WR:
							case FUN_4WR:
								if(measured_value==0xFFFFFFFF)//开路
								{
									fprintf(f, "%s,%s\n", strftime_buf, "open circuit");
								}
								else
								{
									if(unit==0x08)//uΩ
									{
										fprintf(f, "%s,%.2f,%s\n", strftime_buf, (float)measured_value / 1000000.0, "R");
									}
									else if (unit == 0x09) // mR
									{
										fprintf(f, "%s,%.2f,%s\n", strftime_buf, (float)measured_value / 1000.0, "R");
									}
									else if (unit == 0x0A) // R
									{
										fprintf(f, "%s,%.2f,%s\n", strftime_buf, (float)measured_value / 1000.0, "KR");
									}
								}
								break;
							case FUN_DIODE:
								if(measured_value==0xFFFFFFFF)//开路
								{
									fprintf(f, "%s,%s\n", strftime_buf, "open circuit");
								}
								else
								{
									fprintf(f, "%s,%u,%s\n", strftime_buf, measured_value/1000, "mV");
								}
								break;
							case FUN_BEEP:
								if(measured_value==0xFFFFFFFF)//开路
								{
									fprintf(f, "%s,%s\n", strftime_buf, "open circuit");
								}
								else
								{
									fprintf(f, "%s,%.2f,%s\n", strftime_buf, (float)measured_value / 1000.0, "mR");
								}
								break;
							case FUN_DCW:	
							case FUN_ACW:
								if (unit == 0x0C)	// uW
								{
									fprintf(f, "%s,%d,%.2f,%.2f\n", strftime_buf,carried_value1,(float)carried_value2 / 1000.0, (float)measured_value / 1000.0);
								}
								else if (unit == 0x0D) // mW
								{
									fprintf(f, "%s,%d,%.2f,%d\n", strftime_buf,carried_value1,(float)carried_value2 / 1000.0, measured_value);
								}
								break;
							case FUN_TEMP:
								if(measured_value==0xFFFFFFFF)// 超出范围
								{
									fprintf(f, "%s,%s\n", strftime_buf, "out of range");
								}
								else
								{
									sign_ch = sign ? '-' : 0x00; //摄氏度正负号
									fprintf(f, "%s,%.2f,%c%.2f", strftime_buf, (float)carried_value1 / 100.0, sign_ch,(float)measured_value / 1000.0);
									sign_ch = carried_value2_sign ? '-' : 0x00; //华氏度正负号
									fprintf(f, ",%c%.2f\n", sign_ch,(float)carried_value2 / 100.0);
								}
								break;
							case FUN_UART:
								
								break;
							case FUN_RS485:
								
								break;

							default:
								not = 1;
								break;
							}

							if (fclose(f) == -1)
							{
								sd_state = 0;
								evt=REFRESH_LEFT_ICON;
                  				xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
							}
						}
					}
					break;

					case SD_RECEIVE_PORT: //收到串口数据
						time(&now);
						localtime_r(&now, &timeinfo);

						f = fopen(file_name, "a");
						if (f == NULL)
						{
							sd_state = 0;
							evt=REFRESH_LEFT_ICON;
                  			xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
						}
						else
						{
							if (second != timeinfo.tm_sec) //秒变了，另起一行，写入日期时间
							{
								second = timeinfo.tm_sec;
								strftime(strftime_buf, sizeof(strftime_buf), "%Y/%m/%d %X", &timeinfo);
								//printf("time = %s \n", strftime_buf);
								fprintf(f, "\r\n[%s]", strftime_buf);
							}

							fwrite(sd_uart_data,1,sd_uart_data_len,f);
							



							if (fclose(f) == -1)
							{
								sd_state = 0;
								evt=REFRESH_LEFT_ICON;
                  				xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
							}
						}
					break;

					case SD_RECEIVE_CAN: //收到CAN数据
						time(&now);
						localtime_r(&now, &timeinfo);

						if (hour != timeinfo.tm_hour) //如果小时变了，新建一个文件存储
						{
							hour = timeinfo.tm_hour;
							evt = SD_SWFUN;
							xQueueSendFromISR(sd_evt_queue, &evt, NULL);
						}
						else
						{
							f = fopen(file_name, "a");
							if (f == NULL)
							{
								sd_state = 0;
								evt=REFRESH_LEFT_ICON;
								xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
							}
							else
							{
								strftime(strftime_buf, sizeof(strftime_buf), "%Y/%m/%d %X", &timeinfo);
								//printf("time = %s \n", strftime_buf);
								fprintf(f, "%s", strftime_buf);
								if(can_rx_data.can_extd==0)//标准帧
								{
									fprintf(f, ",%s", ch_gbk6);
								}
								else//扩展帧
								{
									fprintf(f, ",%s", ch_gbk7);
								}

								if(can_rx_data.can_rtr==0)//数据帧
								{
									fprintf(f, ",%s", ch_gbk8);
								}
								else//远程帧
								{
									fprintf(f, ",%s", ch_gbk9);
								}
								fprintf(f, ",0x%08X", can_rx_data.identifier);

								fprintf(f, ",%02X %02X %02X %02X %02X %02X %02X %02X\n",can_rx_data.can_data[0],can_rx_data.can_data[1],can_rx_data.can_data[2],can_rx_data.can_data[3],can_rx_data.can_data[4],can_rx_data.can_data[5],can_rx_data.can_data[6],can_rx_data.can_data[7]);
								
							}

							if (fclose(f) == -1)
							{
								sd_state = 0;
								evt=REFRESH_LEFT_ICON;
                  				xQueueSendFromISR(gui_evt_queue, &evt, NULL); 
							}
						}
					break;
				}
			}
		}

		// ESP_LOGI(SD, "SD");

		// if(sd_state!=0)
		// {
		// FILE* f = fopen(MOUNT_POINT"/data.csv", "a");
		// if (f == NULL) {
		// 	sd_state=0;
		// }
		// else
		// {
		// 	w_err=fprintf(f, "CU12345678,电压档,%lld,V\n",dianya);
		// 	ESP_LOGI(SD, "w_err=%d\r\n",w_err);
		// 	fclose(f);
		// 	//ESP_LOGI(SD, "SD write");
		// }
		// }
		// else
		// {
		// 	ESP_LOGI(SD, "SD ERROR");
		// }

		// time(&now);
		// localtime_r(&now, &timeinfo);
		// ESP_LOGE(SD,"tm_year=%d",timeinfo.tm_year+1900);
		// ESP_LOGE(SD,"tm_mon=%d",timeinfo.tm_mon+1);
		// ESP_LOGE(SD,"tm_mday=%d",timeinfo.tm_mday);
		// ESP_LOGE(SD,"tm_hour=%d",timeinfo.tm_hour);
		// ESP_LOGE(SD,"tm_min=%d",timeinfo.tm_min);
		// ESP_LOGE(SD,"tm_sec=%d",timeinfo.tm_sec);
		// ESP_LOGE(SD,"tm_wday=%d",timeinfo.tm_wday);
	}
}
