#include "main.h"
#include "PCF8563.h"

const char *PCF8563_TAG = "PCF8563_TAG";

//ȫ�ֱ���,�����ʼ��ֵ����Ҫ��ʼ����ʱ�䣬
//��;��1����ʼ��ʱ�䡣2����ȡ����ʱ�� 
//                          �룬�֣�ʱ���죬���ڣ��·ݣ���ݡ�
uint8_t PCF8563_Time[7] = {00, 56, 20, 2,   5,    6,   17};
uint8_t VL_value=0; //Ϊ1δ��ʼ��оƬ

void PCF8563_Init(void)
{

	//ʮ������ת����BCD��
	PCF8563_Time[0] =  ((PCF8563_Time[0]/10) << 4) | (PCF8563_Time[0]%10);
	PCF8563_Time[1] =  ((PCF8563_Time[1]/10) << 4) | (PCF8563_Time[1]%10);
	PCF8563_Time[2] =  ((PCF8563_Time[2]/10) << 4) | (PCF8563_Time[2]%10);
	PCF8563_Time[3] =  ((PCF8563_Time[3]/10) << 4) | (PCF8563_Time[3]%10);
//	PCF8563_Time[4] =  ((PCF8563_Time[4]/10 << 4)) | (PCF8563_Time[4]%10);	   //���ڲ���ת��
	PCF8563_Time[5] =  ((PCF8563_Time[5]/10 << 4)) | (PCF8563_Time[5]%10);
	PCF8563_Time[6] =  ((PCF8563_Time[6]/10 << 4)) | (PCF8563_Time[6]%10);
	PCF8563_CLKOUT_1s();

	PCF8563_WriteAdress(0x00, 0x20);	//��ֹRTC source clock
	//��ʼ��PCF8563��ʱ��
        
	// PCF8563_WriteAdress(0x02, PCF8563_Time[0]);
	// PCF8563_WriteAdress(0x03, PCF8563_Time[1]);
	// PCF8563_WriteAdress(0x04, PCF8563_Time[2]);
	// PCF8563_WriteAdress(0x05, PCF8563_Time[3]);
	// PCF8563_WriteAdress(0x06, PCF8563_Time[4]);
	// PCF8563_WriteAdress(0x07, PCF8563_Time[5]);
	// PCF8563_WriteAdress(0x08, PCF8563_Time[6]);	 
		  
	PCF8563_WriteAdress(0x00, 0x00);	//Enable RTC sorce clock 
        PCF8563_WriteAdress(0x00,0x00);
        PCF8563_WriteAdress(0x01,0x1f);
        PCF8563_WriteAdress(0x0d,0x82);
        PCF8563_WriteAdress(0x0e,0x00);
}

void PCF8563_Init_Time(uint8_t *p)
{
    uint8_t timm[3];
	timm[0] =  ((p[0]/10) << 4) | (p[0]%10);
	timm[1] =  ((p[1]/10) << 4) | (p[1]%10);
	timm[2] =  ((p[2]/10) << 4) | (p[2]%10); 

	PCF8563_WriteAdress(0x02, timm[0]);
	PCF8563_WriteAdress(0x03, timm[1]);
	PCF8563_WriteAdress(0x04, timm[2]);
	PCF8563_WriteAdress(0x00, 0x00);
}

void PCF8563_Init_Day(uint8_t *p)
{
    uint8_t timm[4];
	timm[0] =  ((p[0]/10) << 4) | (p[0]%10);
	timm[1] =  ((p[1]/10) << 4) | (p[1]%10);
	timm[2] =  ((p[2]/10) << 4) | (p[2]%10);
        timm[3] =  ((p[3]/10) << 4) | (p[3]%10);
	//timm[3] =  p[3];

	PCF8563_WriteAdress(0x05, timm[0]);
	PCF8563_WriteAdress(0x06, timm[1]);
	PCF8563_WriteAdress(0x07, timm[2]);
	PCF8563_WriteAdress(0x08, timm[3]);
	PCF8563_WriteAdress(0x00, 0x00);
        
}

uint8_t PCF8563_ReaDAdress(uint8_t Adress)
{	
	uint8_t ReadData=0;

	// IIC_Start();
	// IIC_WriteByte(0xa2);  
	// IIC_WaitAck();
	// IIC_WriteByte(Adress);
	// IIC_WaitAck();
	// IIC_Start();
	// IIC_WriteByte(0xa3);  
	// IIC_WaitAck();
	// ReadData = IIC_ReadByte();
	// IIC_Stop();
	return ReadData;
}

void  PCF8563_WriteAdress(uint8_t Adress,uint8_t DataTX)
{

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, 0xa2, 1);
	i2c_master_write_byte(cmd, Adress, 1);
	i2c_master_write_byte(cmd, DataTX, 1);
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
	if (ret != ESP_OK) {
	//	ESP_LOGI(PCF8563_TAG, "PCF8563 WriteAdress ERROR =%d",ret);
	}
   	i2c_cmd_link_delete(cmd);
	// IIC_Start();
	// IIC_WriteByte(0xa2);
	// IIC_WaitAck();
	// IIC_WriteByte(Adress);
	// IIC_WaitAck();
	// IIC_WriteByte(DataTX);
	// IIC_WaitAck();
	// IIC_Stop();
}
//ȡ�ذ˸��ֽڵ�����:�룬�֣�ʱ���죬���ڣ��·ݣ���ݡ�

void PCF8563_ReadTimes(void)
{
	
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);

	i2c_master_write_byte(cmd, 0xa2, 1);
	i2c_master_write_byte(cmd, 0x02, 1);
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, 0xa3, 0x01);

	i2c_master_read(cmd, PCF8563_Time,7,0x00);
	
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 150 / portTICK_RATE_MS);
	if (ret != ESP_OK) {
	//	ESP_LOGI(PCF8563_TAG, "PCF8563 ReadTimes ERROR =%d",ret);
	}
	i2c_cmd_link_delete(cmd);

	if(PCF8563_Time[0]&0x80)
	{
		VL_value=1;
	}
	else
	{
		VL_value=0;
	}


	PCF8563_Time[0]=PCF8563_Time[0]&0x7f;
	PCF8563_Time[1]=PCF8563_Time[1]&0x7f;
	PCF8563_Time[2]=PCF8563_Time[2]&0x3f;
	PCF8563_Time[3]=PCF8563_Time[3]&0x3f;
	PCF8563_Time[4]=PCF8563_Time[4]&0x07;
	PCF8563_Time[5]=PCF8563_Time[5]&0x1f;

	// IIC_Start();
	// IIC_WriteByte(0xa2);
	// IIC_WaitAck();
	// IIC_WriteByte(0x02);
	// IIC_WaitAck();
	// IIC_Start();
	// IIC_WriteByte(0xa3);
	// IIC_WaitAck();
	// PCF8563_Time[0] = IIC_ReadByte()&0x7f;
	// I2C_Ack();
	
	// PCF8563_Time[1] = IIC_ReadByte()&0x7f;
	// I2C_Ack();

	// PCF8563_Time[2] = IIC_ReadByte()&0x3f;
	// I2C_Ack();

	// PCF8563_Time[3] = IIC_ReadByte()&0x3f;
	// I2C_Ack();
	
	// PCF8563_Time[4] = IIC_ReadByte()&0x07;
	// I2C_Ack();
	
	// PCF8563_Time[5] = IIC_ReadByte()&0x1f;
	// I2C_Ack();
	
	// PCF8563_Time[6] = IIC_ReadByte();
	// I2C_NoAck();
	// IIC_Stop();

	PCF8563_Time[0] = ((PCF8563_Time[0]&0xf0)>>4)*10 + (PCF8563_Time[0]&0x0f);
	PCF8563_Time[1] = ((PCF8563_Time[1]&0xf0)>>4)*10 + (PCF8563_Time[1]&0x0f);
	PCF8563_Time[2] = ((PCF8563_Time[2]&0xf0)>>4)*10 + (PCF8563_Time[2]&0x0f);
	 PCF8563_Time[3] = ((PCF8563_Time[3]&0xf0)>>4)*10 + (PCF8563_Time[3]&0x0f);
	PCF8563_Time[4] = ((PCF8563_Time[4]&0xf0)>>4)*10 + (PCF8563_Time[4]&0x0f);
	PCF8563_Time[5] = ((PCF8563_Time[5]&0xf0)>>4)*10 + (PCF8563_Time[5]&0x0f);
	PCF8563_Time[6] = ((PCF8563_Time[6]&0xf0)>>4)*10 + (PCF8563_Time[6]&0x0f);

}

//��CLKOUT�϶�ʱ1S���һ���½������壬������֤����������STM32��GPIO�������룬���ó��½����жϣ���Ƭ��ÿ��1S����һ���ж�
void PCF8563_CLKOUT_1s(void)
{
	PCF8563_WriteAdress(0x01, 0);	//��ֹ��ʱ��������������
	PCF8563_WriteAdress(0x0e, 0);	//�رն�ʱ���ȵ�
//	PCF8563_WriteAdress(0x0e, 0);	//д��1 

	PCF8563_WriteAdress(0x0d, 0x83);  //���������
}
