#include "mainwindow.h"

// SDL3 and SDL3_image headers
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

// cglm headers for Perspective, View, and Model 3D transformation matrices
#include <cglm/cglm.h>
#include <cglm/affine.h>
#include <cglm/cam.h>
#include <cglm/mat4.h>

#include <QDebug>
#include <QOpenGLContext>

// -----------------------------------------------------------------------------
// Textured 3D Cube Shaders with MVP Matrix
// -----------------------------------------------------------------------------
static const char *vertexShaderSource = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

// General Model-View-Projection matrix from cglm
uniform mat4 u_mvp;

void main() {
    gl_Position = u_mvp * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
)";

static const char *fragmentShaderSource = R"(#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D ourTexture;

void main() {
    // Pure texture mapping without lighting (as requested)
    FragColor = texture(ourTexture, TexCoord);
}
)";

// ==========================================
// GLWidget Implementation
// ==========================================
GLWidget::GLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
}

GLWidget::~GLWidget()
{
    makeCurrent();
    if (m_texture)
        glDeleteTextures(1, &m_texture);
    if (m_vao)
        glDeleteVertexArrays(1, &m_vao);
    if (m_posVbo)
        glDeleteBuffers(1, &m_posVbo);
    if (m_uvVbo)
        glDeleteBuffers(1, &m_uvVbo);
    if (m_shaderProgram)
        glDeleteProgram(m_shaderProgram);
    doneCurrent();
}

GLuint GLWidget::createTextureWithSDL3(const char *path)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "rb");
    if (!io)
    {
        qWarning("Failed to open file stream for %s: %s", path, SDL_GetError());
        return 0;
    }

    // 1 (true) will automatically close and free the SDL_IOStream
    SDL_Surface *surface = IMG_Load_IO(io, 1);
    if (!surface)
    {
        qWarning("Image loading failed for %s: %s", path, SDL_GetError());
        return 0;
    }

    // Convert to RGBA ABGR8888 for OpenGL
    SDL_Surface *optimizedSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surface);

    if (!optimizedSurface)
    {
        qWarning("Surface conversion failed: %s", SDL_GetError());
        return 0;
    }

    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, optimizedSurface->w, optimizedSurface->h,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, optimizedSurface->pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    SDL_DestroySurface(optimizedSurface);
    return textureID;
}

void GLWidget::initializeGL()
{
    // Initialize GLAD with Qt's OpenGL proc address loader
    bool ok = gladLoadGLLoader([](const char *name) -> void * {
        return reinterpret_cast<void *>(QOpenGLContext::currentContext()->getProcAddress(name));
    });

    if (!ok)
    {
        qFatal("Failed to initialize GLAD using Qt OpenGL context!");
    }

    // Enable Depth Testing to handle hidden surfaces correctly
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Shaders compilation
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fs);

    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vs);
    glAttachShader(m_shaderProgram, fs);
    glLinkProgram(m_shaderProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // Cache uniform locations
    m_mvpLoc = glGetUniformLocation(m_shaderProgram, "u_mvp");
    glUseProgram(m_shaderProgram);
    glUniform1i(glGetUniformLocation(m_shaderProgram, "ourTexture"), 0);
    glUseProgram(0);

    // -------------------------------------------------------------------------
    // 3D Cube Geometry: 36 vertices (6 faces x 2 triangles x 3 vertices)
    // -------------------------------------------------------------------------
    static const float cubePositions[] = {
        // Front face (Z = +0.5f)
        -0.5f, -0.5f,  0.5f,
        0.5f, -0.5f,  0.5f,
        0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,
        0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,

        // Back face (Z = -0.5f)
        0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        0.5f, -0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        0.5f,  0.5f, -0.5f,

        // Left face (X = -0.5f)
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,

        // Right face (X = +0.5f)
        0.5f, -0.5f,  0.5f,
        0.5f, -0.5f, -0.5f,
        0.5f,  0.5f, -0.5f,
        0.5f, -0.5f,  0.5f,
        0.5f,  0.5f, -0.5f,
        0.5f,  0.5f,  0.5f,

        // Top face (Y = +0.5f)
        -0.5f,  0.5f,  0.5f,
        0.5f,  0.5f,  0.5f,
        0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f,  0.5f,
        0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,

        // Bottom face (Y = -0.5f)
        -0.5f, -0.5f, -0.5f,
        0.5f, -0.5f, -0.5f,
        0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,
        0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f
    };

    // Dedicated UV coordinates buffer (2 floats per vertex = 72 floats)
    // Correctly mapped so the texture appears upright on all cube faces
    static const float cubeUVs[] = {
        // Front
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f,

        // Back
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f,

        // Left
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f,

        // Right
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f,

        // Top
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f,

        // Bottom
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f
    };

    // Vertex Array Object
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    // Position VBO (Location = 0)
    glGenBuffers(1, &m_posVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_posVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubePositions), cubePositions, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    // Dedicated UV Buffer (Location = 1)
    glGenBuffers(1, &m_uvVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_uvVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeUVs), cubeUVs, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Load WebP texture using SDL3_image
    m_texture = createTextureWithSDL3("assets/textures/clear_01.webp");

    // -------------------------------------------------------------------------
    // QTimer Setup for Smooth Rotation at 90 degrees/second
    // -------------------------------------------------------------------------
    m_elapsedTimer.start();
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, QOverload<>::of(&GLWidget::update));
    m_timer->start(16); // ~60 updates per second
}

void GLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void GLWidget::paintGL()
{
    glClearColor(0.2f, 0.25f, 0.3f, 1.0f);
    // Clear both color buffer and depth buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_texture)
    {
        glUseProgram(m_shaderProgram);

        // ---------------------------------------------------------------------
        // Calculate Rotation Angle: 90 degrees per second
        // ---------------------------------------------------------------------
        float elapsedSeconds = m_elapsedTimer.elapsed() / 1000.0f;
        float angleDegrees = elapsedSeconds * 90.0f;

        // Model Matrix (Rotate cube around a normalized 3D axis)
        mat4 model;
        glm_mat4_identity(model);
        // Normalized 3D rotation axis to showcase all faces
        vec3 rotAxis = {0.5f, 1.0f, 0.2f};
        glm_vec3_normalize(rotAxis);
        glm_rotate(model, glm_rad(angleDegrees), rotAxis);

        // View Matrix (Camera eye at 3.5 on Z, looking at origin)
        mat4 view;
        vec3 eye = {0.0f, 0.0f, 3.5f};
        vec3 center = {0.0f, 0.0f, 0.0f};
        vec3 up = {0.0f, 1.0f, 0.0f};
        glm_lookat(eye, center, up, view);

        // Perspective Projection Matrix
        mat4 projection;
        float aspect = (height() > 0) ? (float)width() / (float)height() : 1.0f;
        glm_perspective(glm_rad(45.0f), aspect, 0.1f, 100.0f, projection);

        // General MVP Matrix = Projection * (View * Model)
        mat4 view_model;
        glm_mat4_mul(view, model, view_model);

        mat4 mvp;
        glm_mat4_mul(projection, view_model, mvp);

        // Pass the calculated MVP matrix to the vertex shader
        if (m_mvpLoc != -1)
        {
            glUniformMatrix4fv(m_mvpLoc, 1, GL_FALSE, (const GLfloat *)mvp);
        }

        // Bind texture to unit 0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_texture);

        // Render the 36 cube vertices
        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
    }
}

// ==========================================
// MainWindow Implementation
// ==========================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Qt 6 + OpenGL 3.3 + cglm (Rotating 3D Cube)");
    resize(500, 500);

    m_glWidget = new GLWidget(this);
    setCentralWidget(m_glWidget);
}
