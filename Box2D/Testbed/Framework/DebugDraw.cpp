/*
* Copyright (c) 2006-2013 Erin Catto http://www.box2d.org
*
* This software is provided 'as-is', without any express or implied
* warranty.  In no event will the authors be held liable for any damages
* arising from the use of this software.
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
*/

#include <stdio.h>
#include <stdarg.h>

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include "imgui.h"

#include "DebugDraw.h"

DebugDraw g_debugDraw;
Camera g_camera;

//
b2Vec2 Camera::ConvertScreenToWorld(const b2Vec2& ps)
{
    float32 w = float32(m_width);
    float32 h = float32(m_height);
	float32 u = ps.x / w;
	float32 v = (h - ps.y) / h;

	float32 ratio = w / h;
	b2Vec2 extents(ratio * 25.0f, 25.0f);
	extents *= m_zoom;

	b2Vec2 lower = m_center - extents;
	b2Vec2 upper = m_center + extents;

	b2Vec2 pw;
	pw.x = (1.0f - u) * lower.x + u * upper.x;
	pw.y = (1.0f - v) * lower.y + v * upper.y;
	return pw;
}

//
b2Vec2 Camera::ConvertWorldToScreen(const b2Vec2& pw)
{
	float32 w = float32(m_width);
	float32 h = float32(m_height);
	float32 ratio = w / h;
	b2Vec2 extents(ratio * 25.0f, 25.0f);
	extents *= m_zoom;

	b2Vec2 lower = m_center - extents;
	b2Vec2 upper = m_center + extents;

	float32 u = (pw.x - lower.x) / (upper.x - lower.x);
	float32 v = (pw.y - lower.y) / (upper.y - lower.y);

	b2Vec2 ps;
	ps.x = u * w;
	ps.y = (1.0f - v) * h;
	return ps;
}

// Convert from world coordinates to normalized device coordinates.
// http://www.songho.ca/opengl/gl_projectionmatrix.html
void Camera::BuildProjectionMatrix(float32* m, float32 zBias)
{
	float32 w = float32(m_width);
	float32 h = float32(m_height);
	float32 ratio = w / h;
	b2Vec2 extents(ratio * 25.0f, 25.0f);
	extents *= m_zoom;

	b2Vec2 lower = m_center - extents;
	b2Vec2 upper = m_center + extents;

	m[0] = 2.0f / (upper.x - lower.x);
	m[1] = 0.0f;
	m[2] = 0.0f;
	m[3] = 0.0f;

	m[4] = 0.0f;
	m[5] = 2.0f / (upper.y - lower.y);
	m[6] = 0.0f;
	m[7] = 0.0f;

	m[8] = 0.0f;
	m[9] = 0.0f;
	m[10] = 1.0f;
	m[11] = 0.0f;

	m[12] = -(upper.x + lower.x) / (upper.x - lower.x);
	m[13] = -(upper.y + lower.y) / (upper.y - lower.y);
	m[14] = zBias;
	m[15] = 1.0f;
}

//
static void sCheckGLError()
{
	GLenum errCode = glGetError();
	if (errCode != GL_NO_ERROR)
	{
		fprintf(stderr, "OpenGL error = %d\n", errCode);
		assert(false);
	}
}

// Prints shader compilation errors
static void sPrintLog(GLuint object)
{
	GLint log_length = 0;
	if (glIsShader(object))
		glGetShaderiv(object, GL_INFO_LOG_LENGTH, &log_length);
	else if (glIsProgram(object))
		glGetProgramiv(object, GL_INFO_LOG_LENGTH, &log_length);
	else
	{
		fprintf(stderr, "printlog: Not a shader or a program\n");
		return;
	}

	char* log = (char*)malloc(log_length);

	if (glIsShader(object))
		glGetShaderInfoLog(object, log_length, NULL, log);
	else if (glIsProgram(object))
		glGetProgramInfoLog(object, log_length, NULL, log);

	fprintf(stderr, "%s", log);
	free(log);
}


//
static GLuint sCreateShaderFromString(const char* source, GLenum type)
{
	GLuint res = glCreateShader(type);
	const char* sources[] = { source };
	glShaderSource(res, 1, sources, NULL);
	glCompileShader(res);
	GLint compile_ok = GL_FALSE;
	glGetShaderiv(res, GL_COMPILE_STATUS, &compile_ok);
	if (compile_ok == GL_FALSE)
	{
		fprintf(stderr, "Error compiling shader of type %d!\n", type);
		sPrintLog(res);
		glDeleteShader(res);
		return 0;
	}

	return res;
}

// 
static GLuint sCreateShaderProgram(const char* vs, const char* fs)
{
	GLuint vsId = sCreateShaderFromString(vs, GL_VERTEX_SHADER);
	GLuint fsId = sCreateShaderFromString(fs, GL_FRAGMENT_SHADER);
	assert(vsId != 0 && fsId != 0);

	GLuint programId = glCreateProgram();
	glAttachShader(programId, vsId);
	glAttachShader(programId, fsId);
	glLinkProgram(programId);

	glDeleteShader(vsId);
	glDeleteShader(fsId);

	GLint status = GL_FALSE;
	glGetProgramiv(programId, GL_LINK_STATUS, &status);
	assert(status != GL_FALSE);
	
	return programId;
}

//
struct GLRenderPoints
{
	struct Vertex
	{
		b2Vec2 position;
		b2Color color;
		float size;
	};

	void Create()
	{
		const char* vs = \
        "uniform mat4 projectionMatrix;\n"
        "attribute vec2 v_position;\n"
        "attribute vec4 v_color;\n"
		"attribute float v_size;\n"
        "varying vec4 f_color;\n"
        "void main(void)\n"
        "{\n"
        "	f_color = v_color;\n"
        "	gl_Position = projectionMatrix * vec4(v_position, 0.0, 1.0);\n"
		"   gl_PointSize = v_size;\n"
        "}\n";
        
		const char* fs = \
        "varying vec4 f_color;\n"
        "void main(void)\n"
        "{\n"
        "	gl_FragColor = f_color;\n"
        "}\n";
        
		m_programId = sCreateShaderProgram(vs, fs);
		m_projectionUniform = glGetUniformLocation(m_programId, "projectionMatrix");
		m_positionAttribute = glGetAttribLocation(m_programId, "v_position");
		m_colorAttribute = glGetAttribLocation(m_programId, "v_color");
		m_sizeAttribute = glGetAttribLocation(m_programId, "v_size");
        
		// Generate
		glGenBuffers(1, &m_vboId);
        
		// Vertex buffer
		glBindBuffer(GL_ARRAY_BUFFER, m_vboId);
		glBufferData(GL_ARRAY_BUFFER, sizeof(m_vertices), m_vertices, GL_DYNAMIC_DRAW);
        
		sCheckGLError();
        
		// Cleanup
		glBindBuffer(GL_ARRAY_BUFFER, 0);
        
		m_count = 0;
	}
    
	void Destroy()
	{
		if (m_vboId)
		{
			glDeleteBuffers(1, &m_vboId);
			m_vboId = 0;
		}
        
		if (m_programId)
		{
			glDeleteProgram(m_programId);
			m_programId = 0;
		}
	}
    
	void Vertex(const b2Vec2& v, const b2Color& c, float32 size)
	{
		if (m_count == e_maxVertices)
			Flush();
        
		struct Vertex& vertex = m_vertices[m_count];
		vertex.position = v;
		vertex.color = c;
		vertex.size = size;
		++m_count;
	}
    
    void Flush()
	{
        if (m_count == 0)
            return;
        
		glUseProgram(m_programId);
        
		float32 proj[16] = { 0.0f };
		g_camera.BuildProjectionMatrix(proj, 0.0f);
        
		glUniformMatrix4fv(m_projectionUniform, 1, GL_FALSE, proj);
        
		glBindBuffer(GL_ARRAY_BUFFER, m_vboId);
		glBufferSubData(GL_ARRAY_BUFFER, 0, m_count * sizeof(struct Vertex), m_vertices);

		glEnableVertexAttribArray(m_positionAttribute);
		glVertexAttribPointer(m_positionAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (const void*) offsetof(struct Vertex, position));
		glEnableVertexAttribArray(m_colorAttribute);
		glVertexAttribPointer(m_colorAttribute, 4, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (const void*) offsetof(struct Vertex, color));
		glEnableVertexAttribArray(m_sizeAttribute);
		glVertexAttribPointer(m_sizeAttribute, 1, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (const void*) offsetof(struct Vertex, size));

		glEnable(GL_PROGRAM_POINT_SIZE);
		glDrawArrays(GL_POINTS, 0, m_count);
        glDisable(GL_PROGRAM_POINT_SIZE);

		glDisableVertexAttribArray(m_positionAttribute);
		glDisableVertexAttribArray(m_colorAttribute);
		glDisableVertexAttribArray(m_sizeAttribute);

		sCheckGLError();
        
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glUseProgram(0);
        
		m_count = 0;
	}
    
	enum { e_maxVertices = 512 };
	struct Vertex m_vertices[e_maxVertices];

	int32 m_count;
    
	GLuint m_vboId;
	GLuint m_programId;
	GLint m_projectionUniform;
	GLint m_positionAttribute;
	GLint m_colorAttribute;
	GLint m_sizeAttribute;
};

//
struct GLRenderLines
{
	struct Vertex
	{
		b2Vec2 position;
		b2Color color;
	};

	void Create()
	{
		const char* vs = \
        "uniform mat4 projectionMatrix;\n"
        "attribute vec2 v_position;\n"
        "attribute vec4 v_color;\n"
        "varying vec4 f_color;\n"
        "void main(void)\n"
        "{\n"
        "	f_color = v_color;\n"
        "	gl_Position = projectionMatrix * vec4(v_position, 0.0, 1.0);\n"
        "}\n";
        
		const char* fs = \
        "varying vec4 f_color;\n"
        "void main(void)\n"
        "{\n"
        "	gl_FragColor = f_color;\n"
        "}\n";
        
		m_programId = sCreateShaderProgram(vs, fs);
		m_projectionUniform = glGetUniformLocation(m_programId, "projectionMatrix");
		m_positionAttribute = glGetAttribLocation(m_programId, "v_position");
		m_colorAttribute = glGetAttribLocation(m_programId, "v_color");
        
		// Generate
		glGenBuffers(1, &m_vboId);
        
		// Vertex buffer
		glBindBuffer(GL_ARRAY_BUFFER, m_vboId);
		glBufferData(GL_ARRAY_BUFFER, sizeof(m_vertices), m_vertices, GL_DYNAMIC_DRAW);
        
		sCheckGLError();
        
		// Cleanup
		glBindBuffer(GL_ARRAY_BUFFER, 0);
        
		m_count = 0;
	}
    
	void Destroy()
	{
		if (m_vboId)
		{
			glDeleteBuffers(1, &m_vboId);
			m_vboId = 0;
		}
        
		if (m_programId)
		{
			glDeleteProgram(m_programId);
			m_programId = 0;
		}
	}
    
	void Vertex(const b2Vec2& v, const b2Color& c)
	{
		if (m_count == e_maxVertices)
			Flush();
        
		struct Vertex& vertex = m_vertices[m_count];
		vertex.position = v;
		vertex.color = c;
		++m_count;
	}
    
    void Flush()
	{
        if (m_count == 0)
            return;
        
		glUseProgram(m_programId);
        
		float32 proj[16] = { 0.0f };
		g_camera.BuildProjectionMatrix(proj, 0.1f);
        
		glUniformMatrix4fv(m_projectionUniform, 1, GL_FALSE, proj);
        
		glBindBuffer(GL_ARRAY_BUFFER, m_vboId);
		glBufferSubData(GL_ARRAY_BUFFER, 0, m_count * sizeof(struct Vertex), m_vertices);

		glEnableVertexAttribArray(m_positionAttribute);
		glVertexAttribPointer(m_positionAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (const void*) offsetof(struct Vertex, position));
		glEnableVertexAttribArray(m_colorAttribute);
		glVertexAttribPointer(m_colorAttribute, 4, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (const void*) offsetof(struct Vertex, color));
        
		glDrawArrays(GL_LINES, 0, m_count);

		glDisableVertexAttribArray(m_positionAttribute);
		glDisableVertexAttribArray(m_colorAttribute);
        
		sCheckGLError();
        
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glUseProgram(0);
        
		m_count = 0;
	}
    
	enum { e_maxVertices = 2 * 512 };
	struct Vertex m_vertices[e_maxVertices];
    
	int32 m_count;
    
	GLuint m_vboId;
	GLuint m_programId;
	GLint m_projectionUniform;
	GLint m_positionAttribute;
	GLint m_colorAttribute;
};

//
struct GLRenderTriangles
{
	struct Vertex
	{
		b2Vec2 position;
		b2Color color;
	};

	void Create()
	{
		const char* vs = \
			"uniform mat4 projectionMatrix;\n"
			"attribute vec2 v_position;\n"
			"attribute vec4 v_color;\n"
			"varying vec4 f_color;\n"
			"void main(void)\n"
			"{\n"
			"	f_color = v_color;\n"
			"	gl_Position = projectionMatrix * vec4(v_position, 0.0, 1.0);\n"
			"}\n";

		const char* fs = \
			"varying vec4 f_color;\n"
			"void main(void)\n"
			"{\n"
			"	gl_FragColor = f_color;\n"
			"}\n";

		m_programId = sCreateShaderProgram(vs, fs);
		m_projectionUniform = glGetUniformLocation(m_programId, "projectionMatrix");
		m_positionAttribute = glGetAttribLocation(m_programId, "v_position");
		m_colorAttribute = glGetAttribLocation(m_programId, "v_color");

		// Generate
		glGenBuffers(1, &m_vboId);

		// Vertex buffer
		glBindBuffer(GL_ARRAY_BUFFER, m_vboId);
		glBufferData(GL_ARRAY_BUFFER, sizeof(m_vertices), m_vertices, GL_DYNAMIC_DRAW);

		sCheckGLError();

		// Cleanup
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		m_count = 0;
	}

	void Destroy()
	{
		if (m_vboId)
		{
			glDeleteBuffers(1, &m_vboId);
			m_vboId = 0;
		}

		if (m_programId)
		{
			glDeleteProgram(m_programId);
			m_programId = 0;
		}
	}

	void Vertex(const b2Vec2& v, const b2Color& c)
	{
		if (m_count == e_maxVertices)
			Flush();

		struct Vertex& vertex = m_vertices[m_count];
		vertex.position = v;
		vertex.color = c;
		++m_count;
	}

    void Flush()
	{
        if (m_count == 0)
            return;
        
		glUseProgram(m_programId);
        
		float32 proj[16] = { 0.0f };
		g_camera.BuildProjectionMatrix(proj, 0.2f);
        
		glUniformMatrix4fv(m_projectionUniform, 1, GL_FALSE, proj);
        
		glBindBuffer(GL_ARRAY_BUFFER, m_vboId);
		glBufferSubData(GL_ARRAY_BUFFER, 0, m_count * sizeof(struct Vertex), m_vertices);
        
		glEnableVertexAttribArray(m_positionAttribute);
		glVertexAttribPointer(m_positionAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (const void*) offsetof(struct Vertex, position));
		glEnableVertexAttribArray(m_colorAttribute);
		glVertexAttribPointer(m_colorAttribute, 4, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (const void*) offsetof(struct Vertex, color));

        glEnable(GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDrawArrays(GL_TRIANGLES, 0, m_count);
        glDisable(GL_BLEND);

		glDisableVertexAttribArray(m_positionAttribute);
		glDisableVertexAttribArray(m_colorAttribute);
        
		sCheckGLError();
        
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glUseProgram(0);
        
		m_count = 0;
	}
    
	enum { e_maxVertices = 3 * 512 };
	struct Vertex m_vertices[e_maxVertices];

	int32 m_count;

	GLuint m_vboId;
	GLuint m_programId;
	GLint m_projectionUniform;
	GLint m_positionAttribute;
	GLint m_colorAttribute;
};

//
DebugDraw::DebugDraw()
{
	m_points = NULL;
    m_lines = NULL;
    m_triangles = NULL;
}

//
DebugDraw::~DebugDraw()
{
	b2Assert(m_points == NULL);
	b2Assert(m_lines == NULL);
	b2Assert(m_triangles == NULL);
}

//
void DebugDraw::Create()
{
	m_points = new GLRenderPoints;
	m_points->Create();
	m_lines = new GLRenderLines;
	m_lines->Create();
	m_triangles = new GLRenderTriangles;
	m_triangles->Create();
}

//
void DebugDraw::Destroy()
{
	m_points->Destroy();
	delete m_points;
	m_points = NULL;

	m_lines->Destroy();
	delete m_lines;
	m_lines = NULL;

	m_triangles->Destroy();
	delete m_triangles;
	m_triangles = NULL;
}

//
void DebugDraw::DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
{
    b2Vec2 p1 = vertices[vertexCount - 1];
	for (int32 i = 0; i < vertexCount; ++i)
	{
        b2Vec2 p2 = vertices[i];
		m_lines->Vertex(p1, color);
		m_lines->Vertex(p2, color);
        p1 = p2;
	}
}

//
void DebugDraw::DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
{
	b2Color fillColor(0.5f * color.r, 0.5f * color.g, 0.5f * color.b, 0.5f);

    for (int32 i = 1; i < vertexCount - 1; ++i)
    {
        m_triangles->Vertex(vertices[0], fillColor);
        m_triangles->Vertex(vertices[i], fillColor);
        m_triangles->Vertex(vertices[i+1], fillColor);
    }

    b2Vec2 p1 = vertices[vertexCount - 1];
	for (int32 i = 0; i < vertexCount; ++i)
	{
        b2Vec2 p2 = vertices[i];
		m_lines->Vertex(p1, color);
		m_lines->Vertex(p2, color);
        p1 = p2;
	}
}

//
void DebugDraw::DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color)
{
	const float32 k_segments = 16.0f;
	const float32 k_increment = 2.0f * b2_pi / k_segments;
    float32 sinInc = sinf(k_increment);
    float32 cosInc = cosf(k_increment);
    b2Vec2 r1(1.0f, 0.0f);
    b2Vec2 v1 = center + radius * r1;
	for (int32 i = 0; i < k_segments; ++i)
	{
        // Perform rotation to avoid additional trigonometry.
        b2Vec2 r2;
        r2.x = cosInc * r1.x - sinInc * r1.y;
        r2.y = sinInc * r1.x + cosInc * r1.y;
		b2Vec2 v2 = center + radius * r2;
        m_lines->Vertex(v1, color);
        m_lines->Vertex(v2, color);
        r1 = r2;
        v1 = v2;
	}
}

//
void DebugDraw::DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color)
{
	const float32 k_segments = 16.0f;
	const float32 k_increment = 2.0f * b2_pi / k_segments;
    float32 sinInc = sinf(k_increment);
    float32 cosInc = cosf(k_increment);
    b2Vec2 v0 = center;
    b2Vec2 r1(cosInc, sinInc);
    b2Vec2 v1 = center + radius * r1;
	b2Color fillColor(0.5f * color.r, 0.5f * color.g, 0.5f * color.b, 0.5f);
	for (int32 i = 0; i < k_segments; ++i)
	{
        // Perform rotation to avoid additional trigonometry.
        b2Vec2 r2;
        r2.x = cosInc * r1.x - sinInc * r1.y;
        r2.y = sinInc * r1.x + cosInc * r1.y;
		b2Vec2 v2 = center + radius * r2;
		m_triangles->Vertex(v0, fillColor);
        m_triangles->Vertex(v1, fillColor);
        m_triangles->Vertex(v2, fillColor);
        r1 = r2;
        v1 = v2;
	}

    r1.Set(1.0f, 0.0f);
    v1 = center + radius * r1;
	for (int32 i = 0; i < k_segments; ++i)
	{
        b2Vec2 r2;
        r2.x = cosInc * r1.x - sinInc * r1.y;
        r2.y = sinInc * r1.x + cosInc * r1.y;
		b2Vec2 v2 = center + radius * r2;
        m_lines->Vertex(v1, color);
        m_lines->Vertex(v2, color);
        r1 = r2;
        v1 = v2;
	}

    // Draw a line fixed in the circle to animate rotation.
	b2Vec2 p = center + radius * axis;
	m_lines->Vertex(center, color);
	m_lines->Vertex(p, color);
}

//
void DebugDraw::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color)
{
	m_lines->Vertex(p1, color);
	m_lines->Vertex(p2, color);
}

//
void DebugDraw::DrawTransform(const b2Transform& xf)
{
	const float32 k_axisScale = 0.4f;
    b2Color red(1.0f, 0.0f, 0.0f);
    b2Color green(0.0f, 1.0f, 0.0f);
	b2Vec2 p1 = xf.p, p2;

	m_lines->Vertex(p1, red);
	p2 = p1 + k_axisScale * xf.q.GetXAxis();
	m_lines->Vertex(p2, red);

	m_lines->Vertex(p1, green);
	p2 = p1 + k_axisScale * xf.q.GetYAxis();
	m_lines->Vertex(p2, green);
}

//
void DebugDraw::DrawPoint(const b2Vec2& p, float32 size, const b2Color& color)
{
    m_points->Vertex(p, color, size);
}

//
void DebugDraw::DrawString(int x, int y, const char *string, ...)
{
	va_list arg;
	va_start(arg, string);
	ImGui::Begin("Overlay", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
	ImGui::SetCursorPos(ImVec2(float(x), float(y)));
	ImGui::TextColoredV(ImColor(230, 153, 153, 255), string, arg);
	ImGui::End();
	va_end(arg);
}

//
void DebugDraw::DrawString(const b2Vec2& pw, const char *string, ...)
{
	b2Vec2 ps = g_camera.ConvertWorldToScreen(pw);

	va_list arg;
	va_start(arg, string);
	ImGui::Begin("Overlay", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
	ImGui::SetCursorPos(ImVec2(ps.x, ps.y));
	ImGui::TextColoredV(ImColor(230, 153, 153, 255), string, arg);
	ImGui::End();
	va_end(arg);
}

//
void DebugDraw::DrawAABB(b2AABB* aabb, const b2Color& c)
{
    b2Vec2 p1 = aabb->lowerBound;
    b2Vec2 p2 = b2Vec2(aabb->upperBound.x, aabb->lowerBound.y);
    b2Vec2 p3 = aabb->upperBound;
    b2Vec2 p4 = b2Vec2(aabb->lowerBound.x, aabb->upperBound.y);
    
    m_lines->Vertex(p1, c);
    m_lines->Vertex(p2, c);

    m_lines->Vertex(p2, c);
    m_lines->Vertex(p3, c);

    m_lines->Vertex(p3, c);
    m_lines->Vertex(p4, c);

    m_lines->Vertex(p4, c);
    m_lines->Vertex(p1, c);
}

//
void DebugDraw::Flush()
{
    m_triangles->Flush();
    m_lines->Flush();
    m_points->Flush();
}
