#include "oglw.h"

using namespace OGLW;
using namespace std;

class Renderer : public App {
    public:
        Renderer() : App({"Renderer", false, false, 960, 720}) {}
        void update(float _dt) override;
        void render(float _dt) override;
        void init() override;
    private:
        unique_ptr<Shader> m_shader;
        unique_ptr<Texture> m_texture;
        unique_ptr<RawMesh> m_mesh;
        float m_xrot = 0.f, m_yrot = 0.f;
};
OGLWMain(Renderer);

void Renderer::init() {
    m_camera.setPosition({0.0, 0.0, 10.0});
    m_camera.setNear(5.0);
    m_camera.setFar(15.0);
    m_shader = make_unique<Shader>("default.glsl");
    m_mesh = loadOBJ("19294.24642.16-ao.blend");
    TextureOptions options = {
        GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE,
        {GL_LINEAR, GL_LINEAR},
        {GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE}
    };
    m_texture = make_unique<Texture>("19294.24642.16.png", options);
}

void Renderer::update(float _dt) {
    m_xrot += m_cursorX;
    m_yrot += m_cursorY;
}

void Renderer::render(float _dt) {
    glm::mat4 model, view = m_camera.getViewMatrix();
    model = glm::rotate(model, m_xrot * 1e-2f, glm::vec3(0.0, 1.0, 0.0));
    model = glm::rotate(model, m_yrot * 1e-2f, glm::vec3(1.0, 0.0, 0.0));
    glm::mat4 mvp = m_camera.getProjectionMatrix() * view * model;
    m_texture->bind(0);
    m_shader->setUniform("ao", 0);
    m_shader->setUniform("mvp", mvp);
    RenderState::depthWrite(GL_TRUE);
    RenderState::depthTest(GL_TRUE);
    RenderState::culling(GL_TRUE);
    RenderState::cullFace(GL_BACK);
    m_mesh->draw(*m_shader);
}

