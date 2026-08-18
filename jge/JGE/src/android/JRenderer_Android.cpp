//-------------------------------------------------------------------------------------
//
// JGE++ is a hardware accelerated 2D game SDK for PSP/Windows/Android.
//
// Licensed under the BSD license, see LICENSE in JGE root for details.
//
// Copyright (c) 2007 James Hui (a.k.a. Dr.Watson) <jhkhui@gmail.com>
//
// Android backend: OpenGL ES 1.1 fixed-function renderer.
//
//-------------------------------------------------------------------------------------

#include "../../include/JGE.h"
#include "../../include/JRenderer.h"
#include "../../include/JResourceManager.h"
#include "../../include/JFileSystem.h"
#include "../../include/JAssert.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "CSPSP", __VA_ARGS__)

// stb_image is used for PNG/JPEG/GIF texture decoding.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

namespace {
	// stb_image needs a memory allocator independent of the C runtime file APIs.
}

//------------------------------------------------------------------------------------------------
// JQuad
//------------------------------------------------------------------------------------------------
JQuad::JQuad(JTexture *tex, float x, float y, float width, float height)
		:mTex(tex), mX(x), mY(y), mWidth(width), mHeight(height)
{
	JASSERT(tex != NULL);

	mHotSpotX = 0.0f;
	mHotSpotY = 0.0f;
	for (int i=0;i<4;i++)
		mColor[i].color = 0xFFFFFFFF;

	mHFlipped = false;
	mVFlipped = false;

	SetTextureRect(x, y, width, height);
}

void JQuad::SetTextureRect(float x, float y, float w, float h)
{
	mX = x;
	mY = y;
	mWidth = w;
	mHeight = h;

	mTX0 = x/mTex->mTexWidth;
	mTY0 = y/mTex->mTexHeight;
	mTX1 = (x+w)/mTex->mTexWidth;
	mTY1 = (y+h)/mTex->mTexHeight;
}

void JQuad::GetTextureRect(float *x, float *y, float *w, float *h)
{
	*x=mX; *y=mY; *w=mWidth; *h=mHeight;
}

void JQuad::SetColor(PIXEL_TYPE color)
{
	for (int i=0;i<4;i++)
		mColor[i].color = color;
}

void JQuad::SetHotSpot(float x, float y)
{
	mHotSpotX = x;
	mHotSpotY = y;
}


//------------------------------------------------------------------------------------------------
// JTexture
//------------------------------------------------------------------------------------------------
JTexture::JTexture()
{
	mTexId = 0;
}

JTexture::~JTexture()
{
	if (mTexId != 0)
		glDeleteTextures(1, &mTexId);
}

void JTexture::UpdateBits(int x, int y, int width, int height, PIXEL_TYPE* bits)
{
	JRenderer::GetInstance()->BindTexture(this);
	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bits);
}


//------------------------------------------------------------------------------------------------
// JRenderer
//------------------------------------------------------------------------------------------------
JRenderer* JRenderer::mInstance = NULL;
bool JRenderer::m3DEnabled = false;

void JRenderer::Set3DFlag(bool flag) { m3DEnabled = flag; }

JRenderer* JRenderer::GetInstance()
{
	if (mInstance == NULL)
	{
		mInstance = new JRenderer();
		JASSERT(mInstance != NULL);
		mInstance->InitRenderer();
	}

	return mInstance;
}

void JRenderer::Destroy()
{
	if (mInstance)
	{
		mInstance->DestroyRenderer();
		delete mInstance;
		mInstance = NULL;
	}
}

JRenderer::JRenderer()
{
}

JRenderer::~JRenderer()
{
}

void JRenderer::InitRenderer()
{
	mCurrentTextureFilter = TEX_FILTER_NONE;
	mImageFilter = NULL;

	mCurrTexBlendSrc = BLEND_SRC_ALPHA;
	mCurrTexBlendDest = BLEND_ONE_MINUS_SRC_ALPHA;

	mCurrentTex = 0;
	mFOV = 75.0f;

#ifdef USING_MATH_TABLE
	for (int i=0;i<360;i++)
	{
		mSinTable[i] = sinf(i*DEG2RAD);
		mCosTable[i] = cosf(i*DEG2RAD);
	}
#endif

	mCurrentRenderMode = MODE_UNKNOWN;

	mDisplayWidth = 480;
	mDisplayHeight = 272;
	mViewportX = 0;
	mViewportY = 0;
	mViewportW = 480;
	mViewportH = 272;
}

void JRenderer::DestroyRenderer()
{
}

#if defined(ANDROID)
void JRenderer::SetDisplaySize(int width, int height)
{
	if (width <= 0 || height <= 0) return;

	mDisplayWidth = width;
	mDisplayHeight = height;

	// Aspect-fit letterbox: keep the 480x272 logical framebuffer, scale it up
	// to the largest size that fits the physical surface.
	float sx = (float)width  / SCREEN_WIDTH_F;
	float sy = (float)height / SCREEN_HEIGHT_F;
	float scale = (sx < sy) ? sx : sy;

	int vw = (int)(SCREEN_WIDTH_F * scale + 0.5f);
	int vh = (int)(SCREEN_HEIGHT_F * scale + 0.5f);
	if (vw < 1) vw = 1;
	if (vh < 1) vh = 1;

	mViewportW = vw;
	mViewportH = vh;
	mViewportX = (width - vw) / 2;
	mViewportY = (height - vh) / 2;
}
#endif

void JRenderer::BeginScene()
{
	// Clear the entire physical surface (including any letterbox bars) to black.
	glViewport(0, 0, mDisplayWidth, mDisplayHeight);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Restrict rendering to the aspect-correct region.
	glViewport(mViewportX, mViewportY, mViewportW, mViewportH);
	glLoadIdentity();
}

void JRenderer::EndScene()
{
	glFlush();
}

void JRenderer::BindTexture(JTexture *tex)
{
	if (mCurrentTex != tex->mTexId)
	{
		mCurrentTex = tex->mTexId;
		glBindTexture(GL_TEXTURE_2D, tex->mTexId);

		if (mCurrentTextureFilter == TEX_FILTER_LINEAR)
		{
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		}
		else if (mCurrentTextureFilter == TEX_FILTER_NEAREST)
		{
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		}
	}
}

void JRenderer::EnableTextureFilter(bool flag)
{
	if (flag)
		mCurrentTextureFilter = TEX_FILTER_LINEAR;
	else
		mCurrentTextureFilter = TEX_FILTER_NEAREST;

	mCurrentTex = 0;
}

static void Swapf(float *a, float *b)
{
	float n=*a;
	*a = *b;
	*b = n;
}

void JRenderer::RenderQuad(JQuad* quad, float xo, float yo, float angle, float xScale, float yScale)
{
	float width = quad->mWidth;
	float height = quad->mHeight;
	float x = -quad->mHotSpotX;
	float y = quad->mHotSpotY;

	// Local-space positions (y down, matching the Win/PSP convention).
	GLfloat verts[12]; // 4 vertices * 3 components
	// order: 0 (top-left), 1 (top-right), 2 (bottom-right), 3 (bottom-left)
	float px[4] = { x, x+width, x+width, x };
	float py[4] = { y-height, y-height, y, y };
	for (int i=0;i<4;i++) {
		verts[i*3+0] = px[i];
		verts[i*3+1] = py[i];
		verts[i*3+2] = 0.0f;
	}

	GLfloat uvs[8];
	uvs[0] = quad->mTX0; uvs[1] = quad->mTY1; // 0
	uvs[2] = quad->mTX1; uvs[3] = quad->mTY1; // 1
	uvs[4] = quad->mTX1; uvs[5] = quad->mTY0; // 2
	uvs[6] = quad->mTX0; uvs[7] = quad->mTY0; // 3

	if (quad->mHFlipped)
	{
		Swapf(&uvs[0], &uvs[2]);
		Swapf(&uvs[4], &uvs[6]);
	}

	if (quad->mVFlipped)
	{
		Swapf(&uvs[1], &uvs[5]);
		Swapf(&uvs[3], &uvs[7]);
	}

	BindTexture(quad->mTex);

	yo = SCREEN_HEIGHT_F - yo;

	glPushMatrix();
	glTranslatef(xo, yo, 0.0f);
	glRotatef(-angle*RAD2DEG, 0.0f, 0.0f, 1.0f);
	glScalef(xScale, yScale, 1.0f);

	GLubyte colors[16];
	for (int i=0;i<4;i++) {
		colors[i*4+0] = quad->mColor[i].r;
		colors[i*4+1] = quad->mColor[i].g;
		colors[i*4+2] = quad->mColor[i].b;
		colors[i*4+3] = quad->mColor[i].a;
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glTexCoordPointer(2, GL_FLOAT, 0, uvs);
	glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	glPopMatrix();

	glColor4ub(255, 255, 255, 255);
}

void JRenderer::RenderQuad(JQuad* quad, VertexColor* pt)
{
	GLfloat verts[12];
	for (int i=0;i<4;i++) {
		pt[i].y = SCREEN_HEIGHT_F - pt[i].y;
		quad->mColor[i].color = pt[i].color;
		verts[i*3+0] = pt[i].x;
		verts[i*3+1] = pt[i].y;
		verts[i*3+2] = pt[i].z;
	}

	GLfloat uvs[8];
	uvs[0] = quad->mTX0; uvs[1] = quad->mTY1;
	uvs[2] = quad->mTX1; uvs[3] = quad->mTY1;
	uvs[4] = quad->mTX1; uvs[5] = quad->mTY0;
	uvs[6] = quad->mTX0; uvs[7] = quad->mTY0;

	BindTexture(quad->mTex);

	GLubyte colors[16];
	for (int i=0;i<4;i++) {
		colors[i*4+0] = quad->mColor[i].r;
		colors[i*4+1] = quad->mColor[i].g;
		colors[i*4+2] = quad->mColor[i].b;
		colors[i*4+3] = quad->mColor[i].a;
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glTexCoordPointer(2, GL_FLOAT, 0, uvs);
	glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	glColor4ub(255, 255, 255, 255);
}


void JRenderer::FillRect(float x, float y, float width, float height, PIXEL_TYPE color)
{
	y = SCREEN_HEIGHT_F - y - height;

	JColor col;
	col.color = color;

	GLfloat verts[12] = {
		x,           y+height, 0.0f,
		x,           y,        0.0f,
		x+width,     y,        0.0f,
		x+width,     y+height, 0.0f
	};

	glDisable(GL_TEXTURE_2D);
	glColor4ub(col.r, col.g, col.b, col.a);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableClientState(GL_VERTEX_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glColor4ub(255, 255, 255, 255);
}


void JRenderer::DrawRect(float x, float y, float width, float height, PIXEL_TYPE color)
{
	y = SCREEN_HEIGHT_F - y - height;

	JColor col;
	col.color = color;

	GLfloat verts[8*3] = {
		x,       y,        0.0f,
		x,       y+height, 0.0f,
		x,       y+height, 0.0f,
		x+width, y+height, 0.0f,
		x+width, y+height, 0.0f,
		x+width, y,        0.0f,
		x+width, y,        0.0f,
		x,       y,        0.0f
	};

	glDisable(GL_TEXTURE_2D);
	glColor4ub(col.r, col.g, col.b, col.a);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawArrays(GL_LINES, 0, 8);
	glDisableClientState(GL_VERTEX_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glColor4ub(255, 255, 255, 255);
}


void JRenderer::FillRect(float x, float y, float width, float height, PIXEL_TYPE* colors)
{
	JColor col[4];
	for (int i=0;i<4;i++)
		col[i].color = colors[i];

	FillRect(x, y, width, height, col);
}


void JRenderer::FillRect(float x, float y, float width, float height, JColor* colors)
{
	y = SCREEN_HEIGHT_F - y - height;

	GLubyte cols[16];
	for (int i=0;i<4;i++) {
		cols[i*4+0] = colors[i].r;
		cols[i*4+1] = colors[i].g;
		cols[i*4+2] = colors[i].b;
		cols[i*4+3] = colors[i].a;
	}

	GLfloat verts[12] = {
		x,           y+height, 0.0f,
		x,           y,        0.0f,
		x+width,     y,        0.0f,
		x+width,     y+height, 0.0f
	};

	glDisable(GL_TEXTURE_2D);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glColorPointer(4, GL_UNSIGNED_BYTE, 0, cols);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glColor4ub(255, 255, 255, 255);
}


void JRenderer::DrawLine(float x1, float y1, float x2, float y2, PIXEL_TYPE color)
{
	JColor col;
	col.color = color;

	GLfloat verts[6] = { x1, SCREEN_HEIGHT_F-y1, 0.0f, x2, SCREEN_HEIGHT_F-y2, 0.0f };

	glDisable(GL_TEXTURE_2D);
	glColor4ub(col.r, col.g, col.b, col.a);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawArrays(GL_LINES, 0, 2);
	glDisableClientState(GL_VERTEX_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glColor4ub(255, 255, 255, 255);
}


void JRenderer::Plot(float x, float y, PIXEL_TYPE color)
{
	JColor col;
	col.color = color;

	GLfloat verts[3] = { x, SCREEN_HEIGHT_F-y, 0.0f };

	glDisable(GL_TEXTURE_2D);
	glColor4ub(col.r, col.g, col.b, col.a);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawArrays(GL_POINTS, 0, 1);
	glDisableClientState(GL_VERTEX_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glColor4ub(255, 255, 255, 255);
}


void JRenderer::PlotArray(float *x, float *y, int count, PIXEL_TYPE color)
{
	JColor col;
	col.color = color;

	GLfloat* verts = new GLfloat[count*3];
	for (int i=0;i<count;i++) {
		verts[i*3+0] = x[i];
		verts[i*3+1] = SCREEN_HEIGHT_F-y[i];
		verts[i*3+2] = 0.0f;
	}

	glDisable(GL_TEXTURE_2D);
	glColor4ub(col.r, col.g, col.b, col.a);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawArrays(GL_POINTS, 0, count);
	glDisableClientState(GL_VERTEX_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glColor4ub(255, 255, 255, 255);

	delete[] verts;
}


void JRenderer::ScreenShot(const char* filename)
{
}


static int getNextPower2(int width)
{
	int b = width;
	int n;
	for (n = 0; b != 0; n++) b >>= 1;
	b = 1 << n;
	if (b == 2 * width) b >>= 1;
	return b;
}


JTexture* JRenderer::LoadTexture(const char* filename, int mode)
{
	JFileSystem* fileSystem = JFileSystem::GetInstance();
	if (!fileSystem->OpenFile(filename))
		return NULL;

	int size = fileSystem->GetFileSize();
	if (size <= 0) {
		fileSystem->CloseFile();
		return NULL;
	}

	unsigned char* rawdata = new unsigned char[size];
	fileSystem->ReadFile(rawdata, size);
	fileSystem->CloseFile();

	int width = 0, height = 0, comp = 0;
	unsigned char* pixels = stbi_load_from_memory(rawdata, size, &width, &height, &comp, 4);
	delete[] rawdata;

	if (pixels == NULL) {
		LOGI("LoadTexture FAILED '%s': %s", filename, stbi_failure_reason());
		return NULL;
	}
	LOGI("LoadTexture OK '%s': %dx%d", filename, width, height);

	int tw = getNextPower2(width);
	int th = getNextPower2(height);

	// Resample into a power-of-two buffer (GLES 1.1 requires POT textures).
	unsigned char* pot = NULL;
	if (tw == width && th == height) {
		pot = pixels;
	} else {
		pot = new unsigned char[tw * th * 4];
		memset(pot, 0, tw * th * 4);
		for (int y = 0; y < height; y++) {
			memcpy(pot + y * tw * 4, pixels + y * width * 4, width * 4);
		}
		stbi_image_free(pixels);
		pixels = NULL;
	}

	if (mImageFilter != NULL)
		mImageFilter->ProcessImage((PIXEL_TYPE*)pot, width, height);

	JTexture *tex = new JTexture();
	if (!tex) {
		delete[] pot;
		return NULL;
	}

	tex->mFilter = TEX_FILTER_LINEAR;
	tex->mWidth = width;
	tex->mHeight = height;
	tex->mTexWidth = tw;
	tex->mTexHeight = th;

	GLuint texid = 0;
	glGenTextures(1, &texid);
	tex->mTexId = texid;

	bool ret = false;
	if (texid != 0) {
		glBindTexture(GL_TEXTURE_2D, texid);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, pot);
		ret = true;
	}

	delete[] pot;

	if (!ret) {
		delete tex;
		tex = NULL;
	}

	return tex;
}

void JRenderer::LoadPNG(TextureInfo &textureInfo, const char *filename, int mode)
{
}

void JRenderer::LoadJPG(TextureInfo &textureInfo, const char *filename, int mode)
{
}

void JRenderer::LoadGIF(TextureInfo &textureInfo, const char *filename, int mode)
{
}

int JRenderer::image_readgif(void * handle, TextureInfo &textureInfo, DWORD * bgcolor, InputFunc readFunc, int mode)
{
	return 1;
}


JTexture* JRenderer::CreateTexture(int width, int height, int mode)
{
	JTexture *tex = new JTexture();
	if (tex)
	{
		tex->mFilter = TEX_FILTER_LINEAR;
		tex->mWidth = width;
		tex->mHeight = height;
		tex->mTexWidth = width;
		tex->mTexHeight = height;

		GLuint texid = 0;
		glGenTextures(1, &texid);
		tex->mTexId = texid;

		unsigned char* buffer = new unsigned char[width * height * 4];
		memset(buffer, 0, width * height * 4);

		glBindTexture(GL_TEXTURE_2D, texid);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

		delete[] buffer;

		return tex;
	}
	else
		return NULL;
}


void JRenderer::EnableVSync(bool flag)
{
}


void JRenderer::ClearScreen(PIXEL_TYPE color)
{
	FillRect(0.0f, 0.0f, SCREEN_WIDTH_F, SCREEN_HEIGHT_F, color);
}


void JRenderer::SetTexBlend(int src, int dest)
{
	if (src != mCurrTexBlendSrc || dest != mCurrTexBlendDest)
	{
		mCurrTexBlendSrc = src;
		mCurrTexBlendDest = dest;

		glBlendFunc(src, dest);
	}
}


void JRenderer::SetTexBlendSrc(int src)
{
	if (src != mCurrTexBlendSrc)
	{
		mCurrTexBlendSrc = src;
		glBlendFunc(mCurrTexBlendSrc, mCurrTexBlendDest);
	}
}


void JRenderer::SetTexBlendDest(int dest)
{
	if (dest != mCurrTexBlendDest)
	{
		mCurrTexBlendDest = dest;
		glBlendFunc(mCurrTexBlendSrc, mCurrTexBlendDest);
	}
}


void JRenderer::ResetPrivateVRAM()
{
}


void JRenderer::Enable2D()
{
	if (mCurrentRenderMode == MODE_2D)
		return;

	mCurrentRenderMode = MODE_2D;

	glViewport(mViewportX, mViewportY, mViewportW, mViewportH);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrthof(0.0f, SCREEN_WIDTH_F-1.0f, 0.0f, SCREEN_HEIGHT_F-1.0f, -1.0f, 1.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glColor4ub(255, 255, 255, 255);
}


void JRenderer::Enable3D()
{
	if (!m3DEnabled)
		return;

	if (mCurrentRenderMode == MODE_3D)
		return;

	mCurrentRenderMode = MODE_3D;

	glViewport(mViewportX, mViewportY, mViewportW, mViewportH);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	float aspect = SCREEN_WIDTH_F / SCREEN_HEIGHT_F;
	float fH = tanf(mFOV * DEG2RAD * 0.5f) * 0.5f;
	float fW = fH * aspect;
	glFrustumf(-fW, fW, -fH, fH, 0.5f, 1000.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glEnable(GL_DEPTH_TEST);
}


void JRenderer::SetClip(int x, int y, int width, int height)
{
	// Convert from top-left (game) coordinates to bottom-left (GL) for scissor.
	glScissor(x, SCREEN_HEIGHT - y - height, width, height);
}


void JRenderer::LoadIdentity()
{
	glLoadIdentity();
}


void JRenderer::Translate(float x, float y, float z)
{
	glTranslatef(x, y, z);
}


void JRenderer::RotateX(float angle)
{
	glRotatef(angle*RAD2DEG, 1.0f, 0.0f, 0.0f);
}


void JRenderer::RotateY(float angle)
{
	glRotatef(angle*RAD2DEG, 0.0f, 1.0f, 0.0f);
}


void JRenderer::RotateZ(float angle)
{
	glRotatef(angle*RAD2DEG, 0.0f, 0.0f, 1.0f);
}


void JRenderer::PushMatrix()
{
	glPushMatrix();
}


void JRenderer::PopMatrix()
{
	glPopMatrix();
}


void JRenderer::RenderTriangles(JTexture* texture, Vertex3D *vertices, int start, int count)
{
	if (texture)
		BindTexture(texture);

	GLfloat* verts = new GLfloat[count*3*3];
	GLfloat* uvs = new GLfloat[count*3*2];
	int index = start*3;
	for (int i = 0; i < count*3; i++)
	{
		verts[i*3+0] = vertices[index].x;
		verts[i*3+1] = vertices[index].y;
		verts[i*3+2] = vertices[index].z;
		uvs[i*2+0] = vertices[index].u;
		uvs[i*2+1] = vertices[index].v;
		index++;
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glTexCoordPointer(2, GL_FLOAT, 0, uvs);
	glDrawArrays(GL_TRIANGLES, 0, count*3);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	delete[] verts;
	delete[] uvs;
}


void JRenderer::SetFOV(float fov)
{
	mFOV = fov;
}


void JRenderer::FillPolygon(float* x, float* y, int count, PIXEL_TYPE color)
{
	JColor col;
	col.color = color;

	GLfloat* verts = new GLfloat[count*3];
	for (int i=0;i<count;i++) {
		verts[i*3+0] = x[i];
		verts[i*3+1] = SCREEN_HEIGHT_F-y[i];
		verts[i*3+2] = 0.0f;
	}

	glDisable(GL_TEXTURE_2D);
	glColor4ub(col.r, col.g, col.b, col.a);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, count);
	glDisableClientState(GL_VERTEX_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glColor4ub(255, 255, 255, 255);

	delete[] verts;
}


void JRenderer::DrawPolygon(float* x, float* y, int count, PIXEL_TYPE color)
{
	JColor col;
	col.color = color;

	GLfloat* verts = new GLfloat[(count+1)*3];
	for (int i=0;i<count;i++) {
		verts[i*3+0] = x[i];
		verts[i*3+1] = SCREEN_HEIGHT_F-y[i];
		verts[i*3+2] = 0.0f;
	}
	verts[count*3+0] = x[0];
	verts[count*3+1] = SCREEN_HEIGHT_F-y[0];
	verts[count*3+2] = 0.0f;

	glDisable(GL_TEXTURE_2D);
	glColor4ub(col.r, col.g, col.b, col.a);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawArrays(GL_LINE_STRIP, 0, count+1);
	glDisableClientState(GL_VERTEX_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glColor4ub(255, 255, 255, 255);

	delete[] verts;
}


void JRenderer::DrawLine(float x1, float y1, float x2, float y2, float lineWidth, PIXEL_TYPE color)
{
	float dy=y2-y1;
	float dx=x2-x1;
	if(dy==0 && dx==0)
		return;

	float l=(float)hypotf(dx,dy);

	float x[4];
	float y[4];

	x[0]=x1+lineWidth*(y2-y1)/l;
	y[0]=y1-lineWidth*(x2-x1)/l;

	x[1]=x1-lineWidth*(y2-y1)/l;
	y[1]=y1+lineWidth*(x2-x1)/l;

	x[2]=x2-lineWidth*(y2-y1)/l;
	y[2]=y2+lineWidth*(x2-x1)/l;

	x[3]=x2+lineWidth*(y2-y1)/l;
	y[3]=y2-lineWidth*(x2-x1)/l;

	FillPolygon(x, y, 4, color);
}


void JRenderer::DrawCircle(float x, float y, float radius, PIXEL_TYPE color)
{
	JColor col;
	col.color = color;

	GLfloat* verts = new GLfloat[(360/2+1)*3];
	int n = 0;
	for (int i=0; i<360; i+=2) {
		verts[n*3+0] = x+radius*COSF(i);
		verts[n*3+1] = SCREEN_HEIGHT_F-y+radius*SINF(i);
		verts[n*3+2] = 0.0f;
		n++;
	}
	verts[n*3+0] = x+radius*COSF(0);
	verts[n*3+1] = SCREEN_HEIGHT_F-y+radius*SINF(0);
	verts[n*3+2] = 0.0f;
	n++;

	glDisable(GL_TEXTURE_2D);
	glColor4ub(col.r, col.g, col.b, col.a);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawArrays(GL_LINE_STRIP, 0, n);
	glDisableClientState(GL_VERTEX_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glColor4ub(255, 255, 255, 255);

	delete[] verts;
}

void JRenderer::FillCircle(float x, float y, float radius, PIXEL_TYPE color)
{
	JColor col;
	col.color = color;

	GLfloat* verts = new GLfloat[(360/2+2)*3];
	int n = 0;
	verts[n*3+0] = x;
	verts[n*3+1] = SCREEN_HEIGHT_F-y;
	verts[n*3+2] = 0.0f;
	n++;
	for (int i=0; i<360; i+=2) {
		verts[n*3+0] = x+radius*COSF(i);
		verts[n*3+1] = SCREEN_HEIGHT_F-y+radius*SINF(i);
		verts[n*3+2] = 0.0f;
		n++;
	}
	verts[n*3+0] = x+radius*COSF(0);
	verts[n*3+1] = SCREEN_HEIGHT_F-y+radius*SINF(0);
	verts[n*3+2] = 0.0f;
	n++;

	glDisable(GL_TEXTURE_2D);
	glColor4ub(col.r, col.g, col.b, col.a);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, n);
	glDisableClientState(GL_VERTEX_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glColor4ub(255, 255, 255, 255);

	delete[] verts;
}


void JRenderer::DrawPolygon(float x, float y, float size, int count, float startingAngle, PIXEL_TYPE color)
{
	JColor col;
	col.color = color;

	float angle = -startingAngle*RAD2DEG;
	float steps = 360.0f/count;
	size /= 2;

	GLfloat* verts = new GLfloat[count*3];
	for (int i=0; i<count; i++) {
		verts[i*3+0] = x+size*COSF((int)angle);
		verts[i*3+1] = SCREEN_HEIGHT_F-(y+size*SINF((int)angle));
		verts[i*3+2] = 0.0f;
		angle += steps;
		if (angle >= 360.0f)
			angle -= 360.0f;
	}

	glDisable(GL_TEXTURE_2D);
	glColor4ub(col.r, col.g, col.b, col.a);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawArrays(GL_LINE_LOOP, 0, count);
	glDisableClientState(GL_VERTEX_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glColor4ub(255, 255, 255, 255);

	delete[] verts;
}


void JRenderer::FillPolygon(float x, float y, float size, int count, float startingAngle, PIXEL_TYPE color)
{
	JColor col;
	col.color = color;

	float angle = -startingAngle*RAD2DEG;
	float firstAngle = angle;
	float steps = 360.0f/count;
	size /= 2;

	GLfloat* verts = new GLfloat[(count+2)*3];
	int n = 0;
	verts[n*3+0] = x;
	verts[n*3+1] = SCREEN_HEIGHT_F-y;
	verts[n*3+2] = 0.0f;
	n++;

	for (int i=0; i<count; i++) {
		verts[n*3+0] = x+size*COSF((int)angle);
		verts[n*3+1] = SCREEN_HEIGHT_F-y+size*SINF((int)angle);
		verts[n*3+2] = 0.0f;
		angle += steps;
		if (angle >= 360.0f)
			angle -= 360.0f;
		n++;
	}

	verts[n*3+0] = x+size*COSF((int)firstAngle);
	verts[n*3+1] = SCREEN_HEIGHT_F-y+size*SINF((int)firstAngle);
	verts[n*3+2] = 0.0f;
	n++;

	glDisable(GL_TEXTURE_2D);
	glColor4ub(col.r, col.g, col.b, col.a);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, n);
	glDisableClientState(GL_VERTEX_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glColor4ub(255, 255, 255, 255);

	delete[] verts;
}


void JRenderer::SetImageFilter(JImageFilter* imageFilter)
{
	mImageFilter = imageFilter;
}


void JRenderer::DrawRoundRect(float x, float y, float w, float h, float radius, PIXEL_TYPE color)
{
	x+=w+radius;
	y+=h+radius;
	JColor col;
	col.color = color;

	glDisable(GL_TEXTURE_2D);
	glColor4ub(col.r, col.g, col.b, col.a);

	int i;
	GLfloat* verts = new GLfloat[(90 + w + 90 + h + 90 + w + 90 + h + 8) * 3];
	int n = 0;
	#define RR_PUSH(xx,yy) verts[n*3+0]=(xx); verts[n*3+1]=SCREEN_HEIGHT_F-(yy); verts[n*3+2]=0.0f; n++;

	for(i=0; i<90;i++) RR_PUSH(x+radius*COSF(i), y+radius*SINF(i));
	for(i=0; i<w; i++)   RR_PUSH(x+radius*COSF(90)-i, y+radius*SINF(90));
	for(i=90; i<180;i++) RR_PUSH(x+radius*COSF(i)-w, y+radius*SINF(i));
	for(i=0; i<h; i++)   RR_PUSH(x+radius*COSF(180)-w, y+radius*SINF(180)-i);
	for(i=180; i<270;i++)RR_PUSH(x+radius*COSF(i)-w, y+radius*SINF(i)-h);
	for(i=0; i<w; i++)   RR_PUSH(x+radius*COSF(270)-w+i, y+radius*SINF(270)-h);
	for(i=270; i<360;i++)RR_PUSH(x+radius*COSF(i), y+radius*SINF(i)-h);
	for(i=0; i<h; i++)   RR_PUSH(x+radius*COSF(0), y+radius*SINF(0)-h+i);
	#undef RR_PUSH

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawArrays(GL_LINE_LOOP, 0, n);
	glDisableClientState(GL_VERTEX_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glColor4ub(255, 255, 255, 255);

	delete[] verts;
}


void JRenderer::FillRoundRect(float x, float y, float w, float h, float radius, PIXEL_TYPE color)
{
	x+=w+radius;
	y+=radius;

	JColor col;
	col.color = color;

	glDisable(GL_TEXTURE_2D);
	glColor4ub(col.r, col.g, col.b, col.a);

	int i;
	GLfloat* verts = new GLfloat[(1 + 90 + w + 90 + h + 90 + w + 90 + h + 2) * 3];
	int n = 0;
	#define FRR_PUSH(xx,yy) verts[n*3+0]=(xx); verts[n*3+1]=SCREEN_HEIGHT_F-(yy); verts[n*3+2]=0.0f; n++;

	FRR_PUSH(x-5, y);
	for(i=0; i<90;i++)  FRR_PUSH(x+radius*COSF(i), y+radius*SINF(i));
	for(i=0; i<w; i++)   FRR_PUSH(x+radius*COSF(90)-i, y+radius*SINF(90));
	for(i=90; i<180;i++) FRR_PUSH(x+radius*COSF(i)-w, y+radius*SINF(i));
	for(i=0; i<h; i++)   FRR_PUSH(x+radius*COSF(180)-w, y+radius*SINF(180)-i);
	for(i=180; i<270;i++)FRR_PUSH(x+radius*COSF(i)-w, y+radius*SINF(i)-h);
	for(i=0; i<w; i++)   FRR_PUSH(x+radius*COSF(270)-w+i, y+radius*SINF(270)-h);
	for(i=270; i<360;i++)FRR_PUSH(x+radius*COSF(i), y+radius*SINF(i)-h);
	for(i=0; i<h; i++)   FRR_PUSH(x+radius*COSF(0), y+radius*SINF(0)-h+i);
	FRR_PUSH(x+radius*COSF(0), y+radius*SINF(0));
	#undef FRR_PUSH

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, n);
	glDisableClientState(GL_VERTEX_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glColor4ub(255, 255, 255, 255);

	delete[] verts;
}
