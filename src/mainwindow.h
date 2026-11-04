#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// glad.h MUST be included before ANY Qt OpenGL headers!
#include <glad/glad.h>

// cglm headers for 3D math (Perspective, View, Model matrices)
#include <cglm/cglm.h>
#include <cglm/affine.h>
#include <cglm/cam.h>
#include <cglm/mat4.h>

#include <QMainWindow>
#include <QOpenGLContext>
#include <QOpenGLWidget>
#include <QTimer>
#include <QElapsedTimer>

class GLWidget : public QOpenGLWidget
{
    Q_OBJECT
public:
    explicit GLWidget(QWidget *parent = nullptr);
    ~GLWidget() override;

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    GLuint createTextureWithSDL3(const char *path);

    GLuint m_shaderProgram = 0;
    GLuint m_vao = 0;
    GLuint m_posVbo = 0;      // Dedicated VBO for 3D vertex positions
    GLuint m_uvVbo = 0;       // Dedicated UV buffer for texture coordinates
    GLuint m_texture = 0;

    GLint m_mvpLoc = -1;      // Uniform location for u_mvp matrix

    QTimer *m_timer = nullptr;
    QElapsedTimer m_elapsedTimer; // Accurate delta time for smooth 90 deg/s rotation
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    GLWidget *m_glWidget = nullptr;
};

#endif // MAINWINDOW_H
