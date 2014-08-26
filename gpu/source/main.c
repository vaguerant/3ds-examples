#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <3ds/types.h>
#include <3ds/srv.h>
#include <3ds/APT.h>
#include <3ds/GSP.h>
#include <3ds/GX.h>
#include <3ds/GPU.h>
#include <3ds/HID.h>
#include <3ds/SHDR.h>
#include <3ds/svc.h>
#include <3ds/os.h>
#include <3ds/gfx.h>
#include "costable.h"
#include "test_shbin.h"
#include "test_png_bin.h"
#include "mdl.h"

DVLB_s* shader;
float* vertArray;
u32* texData;

void loadIdentity44(float* m)
{
	if(!m)return;

	memset(m, 0x00, 16*4);
	m[0]=m[5]=m[10]=m[15]=1.0f;
}

void multMatrix44(float* m1, float* m2, float* m) //4x4
{
	int i, j;
	for(i=0;i<4;i++)for(j=0;j<4;j++)m[i+j*4]=(m1[0+j*4]*m2[i+0*4])+(m1[1+j*4]*m2[i+1*4])+(m1[2+j*4]*m2[i+2*4])+(m1[3+j*4]*m2[i+3*4]);
}

void translateMatrix(float* tm, float x, float y, float z)
{
	float rm[16], m[16];

	loadIdentity44(rm);
	rm[3]=x;
	rm[7]=y;
	rm[11]=z;
	
	multMatrix44(rm,tm,m);
	memcpy(tm,m,16*sizeof(float));
}

void rotateMatrixX(float* tm, float x)
{
	float rm[16], m[16];
	memset(rm, 0x00, 16*4);
	rm[0]=1.0f;
	rm[5]=cos(x);
	rm[6]=sin(x);
	rm[9]=-sin(x);
	rm[10]=cos(x);
	rm[15]=1.0f;
	multMatrix44(tm,rm,m);
	memcpy(tm,m,16*sizeof(float));
}

void rotateMatrixZ(float* tm, float x)
{
	float rm[16], m[16];
	memset(rm, 0x00, 16*4);
	rm[0]=cos(x);
	rm[1]=sin(x);
	rm[4]=-sin(x);
	rm[5]=cos(x);
	rm[10]=1.0f;
	rm[15]=1.0f;
	multMatrix44(tm,rm,m);
	memcpy(tm,m,16*sizeof(float));
}

void scaleMatrix(float* tm, float x, float y, float z)
{
	tm[0]*=x; tm[4]*=x; tm[8]*=x; tm[12]*=x;
	tm[1]*=y; tm[5]*=y; tm[9]*=y; tm[13]*=y;
	tm[2]*=z; tm[6]*=z; tm[10]*=z; tm[14]*=z;
}

void initProjectionMatrix(float* m, float fovy, float aspect, float near, float far)
{
	float top = near*tan(fovy/2);
	float right = (top*aspect);
	
	*(m++) = near/right;
	*(m++) = 0.0f;
	*(m++) = 0.0f;
	*(m++) = 0.0f;

	*(m++) = 0.0f;
	*(m++) = near/top;
	*(m++) = 0.0f;
	*(m++) = 0.0f;

	*(m++) = 0.0f;
	*(m++) = 0.0f;
	// *(m++) = -(far+near)/(far-near);
	*(m++) = 0.0f;
	// *(m++) = -2.0f*(far*near)/(far-near);
	// *(m++) = 1.0f;
	*(m++) = -1.0f;

	*(m++) = 0.0f;
	*(m++) = 0.0f;
	*(m++) = -1.0f;
	*(m++) = 0.0f;
}

void setUniformMatrix(u32 startreg, float* m)
{
	float param[16];

	param[0x0]=m[3]; //w
	param[0x1]=m[2]; //z
	param[0x2]=m[1]; //y
	param[0x3]=m[0]; //x

	param[0x4]=m[7];
	param[0x5]=m[6];
	param[0x6]=m[5];
	param[0x7]=m[4];
	
	param[0x8]=m[11];
	param[0x9]=m[10];
	param[0xa]=m[9];
	param[0xb]=m[8];

	param[0xc]=m[15];
	param[0xd]=m[14];
	param[0xe]=m[13];
	param[0xf]=m[12];

	GPU_SetUniform(startreg, (u32*)param, 4);
}

float angle=0.0f;
float angleZ=0.0f;
float tx, ty, tz;

u32* gpuOut=(u32*)0x1F119400;
u32* gpuDOut=(u32*)0x1F370800;

// topscreen
void doFrame1()
{
	static u32 zero[0x400];
	memset(zero, 0x00, 0x400*4);

	//general setup
		GPU_SetViewport((u32*)osConvertVirtToPhys((u32)gpuDOut),(u32*)osConvertVirtToPhys((u32)gpuOut),0,0,240*2,400);

		GPU_DepthRange(-1.0f, 0.0f);

		GPU_SetFaceCulling(GPU_CULL_BACK_CCW);
		GPU_SetStencilTest(false, GPU_ALWAYS, 0x00);
		GPU_SetDepthTest(true, GPU_GREATER, 0x1F);

	// ?
		GPUCMD_AddSingleParam(0x00010062, 0x00000000); //param always 0x0 according to code
		GPUCMD_AddSingleParam(0x000F0118, 0x00000000);

	//setup shader
		SHDR_UseProgram(shader, 0);

	//attribute buffers
		GPU_SetAttributeBuffers(3, (u32*)osConvertVirtToPhys((u32)vertArray),
			GPU_ATTRIBFMT(0, 3, GPU_FLOAT)|GPU_ATTRIBFMT(1, 2, GPU_FLOAT)|GPU_ATTRIBFMT(2, 3, GPU_FLOAT),
			0xFFC, 0x210, 1, (u32[]){0x00000000}, (u64[]){0x210}, (u8[]){3});

	//?
		GPUCMD_AddSingleParam(0x000F0100, 0x00E40100);
		GPUCMD_AddSingleParam(0x000F0101, 0x01010000);
		GPUCMD_AddSingleParam(0x000F0104, 0x00000010);
	
	//texturing stuff
		GPUCMD_AddSingleParam(0x0002006F, 0x00000100);
		GPUCMD_AddSingleParam(0x000F0080, 0x00011001); //enables/disables texturing
	
	//texenv
		GPU_SetTexEnv(3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00000000);
		GPU_SetTexEnv(4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00000000);
		GPU_SetTexEnv(5, GPU_TEVSOURCES(GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR), GPU_TEVSOURCES(GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR),
			GPU_TEVOPERANDS(0,0,0), GPU_TEVOPERANDS(0,0,0), GPU_MODULATE, GPU_MODULATE, 0xFFFFFFFF);

	//texturing stuff
		GPU_SetTexture((u32*)osConvertVirtToPhys((u32)texData),256,256,0x6,GPU_RGBA8);

	//setup matrices
		float modelView[16];
		float projection[16];

		loadIdentity44(modelView);
		loadIdentity44(projection);

		translateMatrix(modelView, tx, ty, tz);
		rotateMatrixX(modelView, angle);
		rotateMatrixZ(modelView, angleZ);

		initProjectionMatrix(projection, 1.3962634f, 240.0f/400.0f, 0.01f, 10.0f);

		setUniformMatrix(0x20, modelView);
		setUniformMatrix(0x80, projection);

	//draw first model
		GPU_DrawArray(GPU_TRIANGLES, mdlFaces*3);
		// GPU_DrawElements(GPU_TRIANGLES, (u32*)(((u32)((void*)indArray-(void*)gspHeap))+0x20000000-base), 6);

	//setup matrices
		loadIdentity44(modelView);
		loadIdentity44(projection);

		translateMatrix(modelView, tx, -ty, tz);
		rotateMatrixX(modelView, -angle);
		rotateMatrixZ(modelView, -angleZ);

		setUniformMatrix(0x20, modelView);

	//draw second
		GPU_DrawArray(GPU_TRIANGLES, mdlFaces*3);

	//finalize stuff ?
		GPUCMD_AddSingleParam(0x000F0111, 0x00000001);
		GPUCMD_AddSingleParam(0x000F0110, 0x00000001);
		GPUCMD_AddSingleParam(0x0008025E, 0x00000000);
}

void demoControls(void)
{
	hidScanInput();
	u32 PAD=hidKeysHeld();

	if(PAD&KEY_UP)tx+=0.1f;
	if(PAD&KEY_DOWN)tx-=0.1f;

	if(PAD&KEY_LEFT)ty+=0.1f;
	if(PAD&KEY_RIGHT)ty-=0.1f;

	if(PAD&KEY_R)tz+=0.1f;
	if(PAD&KEY_L)tz-=0.1f;

	if(PAD&KEY_A)angle+=0.1f;
	if(PAD&KEY_Y)angle-=0.1f;

	if(PAD&KEY_X)angleZ+=0.1f;
	if(PAD&KEY_B)angleZ-=0.1f;
}

extern u32* gxCmdBuf;

int main()
{
	srvInit();	
	aptInit();
	gfxInit();
	hidInit(NULL);
	aptSetupEventHandler();
	
	GPU_Init(NULL);

	u32 gpuCmdSize=0x40000;
	u32* gpuCmd=(u32*)gfxAllocLinear(gpuCmdSize*4);

	GPU_Reset(gxCmdBuf, gpuCmd, gpuCmdSize);

	vertArray=(float*)gfxAllocLinear(0x100000);
	texData=(u32*)gfxAllocLinear(0x100000);

	memcpy(texData, test_png_bin, test_png_bin_size);
	memcpy(vertArray, mdlData, sizeof(mdlData));

	tx=ty=0.0f; tz=-0.1f;
	shader=SHDR_ParseSHBIN((u32*)test_shbin,test_shbin_size);

	GX_SetMemoryFill(gxCmdBuf, (u32*)gpuOut, 0x404040FF, (u32*)&gpuOut[0x2EE00], 0x201, (u32*)gpuDOut, 0x00000000, (u32*)&gpuDOut[0x2EE00], 0x201);
	gfxSwapBuffersGpu();

	APP_STATUS status;
	while((status=aptGetStatus())!=APP_EXITING)
	{
		if(status==APP_RUNNING)
		{
			demoControls();

			GX_SetMemoryFill(gxCmdBuf, (u32*)gpuOut, 0x404040FF, (u32*)&gpuOut[0x2EE00], 0x201, (u32*)gpuDOut, 0x00000000, (u32*)&gpuDOut[0x2EE00], 0x201);

			GPUCMD_SetBuffer(gpuCmd, gpuCmdSize, 0);
			doFrame1();
			GPUCMD_Finalize();
			GPUCMD_Run(gxCmdBuf);

			gfxSwapBuffersGpu();
			GX_SetDisplayTransfer(gxCmdBuf, (u32*)gpuOut, 0x019001E0, (u32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), 0x019001E0, 0x01001000);
		}
		gspWaitForVBlank();
	}

	hidExit();
	gfxExit();
	aptExit();
	srvExit();
	return 0;
}
