#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
unsigned int loadCubemap(vector<std::string> faces);
void renderQuad();

// settings
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;
// blinn is true and the non-blinn option is disabled in processInput
bool blinn = true;
//bool blinnKeyPressed = false;
// shadows
bool shadows = true;
bool shadowsKeyPressed = false;
// bloom
bool bloom = true;
bool bloomKeyPressed = false;
float exposure = 1.0f;
//lights
unsigned int NR_LIGHTS = 5;

// camera
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct PointLight {
    glm::vec3 position;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    float constant;
    float linear;
    float quadratic;
};
struct ProgramState {
    glm::vec3 clearColor = glm::vec3(0);
    bool ImGuiEnabled = false;
    Camera camera;
    bool CameraMouseMovementUpdateEnabled = true;
    glm::vec3 backpackPosition = glm::vec3(0.0f);
    float backpackScale = 1.0f;
    PointLight pointLight;
    ProgramState()
            : camera(glm::vec3(0.0f, 0.0f, 0.0f)) {}

    void SaveToFile(std::string filename);

    void LoadFromFile(std::string filename);
};
void ProgramState::SaveToFile(std::string filename) {
    std::ofstream out(filename);
    out << clearColor.r << '\n'
        << clearColor.g << '\n'
        << clearColor.b << '\n'
        << ImGuiEnabled << '\n'
        << camera.Position.x << '\n'
        << camera.Position.y << '\n'
        << camera.Position.z << '\n'
        << camera.Front.x << '\n'
        << camera.Front.y << '\n'
        << camera.Front.z << '\n';
}
void ProgramState::LoadFromFile(std::string filename) {
    std::ifstream in(filename);
    if (in) {
        in >> clearColor.r
           >> clearColor.g
           >> clearColor.b
           >> ImGuiEnabled
           >> camera.Position.x
           >> camera.Position.y
           >> camera.Position.z
           >> camera.Front.x
           >> camera.Front.y
           >> camera.Front.z;
    }
}
ProgramState *programState;

void DrawImGui(ProgramState *programState);

int main() {
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    // stbi_set_flip_vertically_on_load(true);

    programState = new ProgramState;
    programState->LoadFromFile("resources/program_state.txt");
    if (programState->ImGuiEnabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    // Init Imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;



    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // configure global opengl state
    // depth test, blend, face culling
    // -----------------------------
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // build and compile shaders
    // -------------------------
    Shader ourShader("resources/shaders/2.model_lighting.vs", "resources/shaders/2.model_lighting.fs");
    Shader skyboxShader("resources/shaders/skybox.vs", "resources/shaders/skybox.fs");
    Shader depthShader("resources/shaders/point_shadows.vs", "resources/shaders/point_shadows.fs", "resources/shaders/point_shadows.gs");
    Shader shaderBlur("resources/shaders/blur.vs", "resources/shaders/blur.fs");
    Shader shaderBloomFinal("resources/shaders/bloom_final.vs", "resources/shaders/bloom_final.fs");

    // depth
    const unsigned int SHADOW_WIDTH = 1024;
    const unsigned int SHADOW_HEIGHT = 1024;
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);
    // create depth cubemap texture
    unsigned int depthCubemap;
    glGenTextures(1, &depthCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
    for (unsigned int i = 0; i < 6; ++i)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    // attach depth texture as FBO's depth buffer
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // configure (floating point) framebuffers
    // ---------------------------------------
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    // create 2 floating point color buffers (1 for normal rendering, other for brightness threshold values)
    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // attach texture to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }
    // create and attach depth buffer (renderbuffer)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ping-pong-framebuffer for blurring
    unsigned int pingpongFBO[2];
    unsigned int pingpongColorbuffers[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
        // also check if framebuffers are complete (no need for depth buffer)
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Framebuffer not complete!" << std::endl;
    }

    ourShader.use();
    ourShader.setInt("material.texture_diffuse1", 0);
    ourShader.setInt("depthMap", 1);
    ourShader.setInt("NR_LIGHTS", NR_LIGHTS);
    shaderBlur.use();
    shaderBlur.setInt("image", 0);
    shaderBloomFinal.use();
    shaderBloomFinal.setInt("scene", 0);
    shaderBloomFinal.setInt("bloomBlur", 1);

    float skyboxVertices[] = {
            // positions
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f,  1.0f
    };

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    vector<std::string> faces
    {
            FileSystem::getPath("resources/textures/skybox/right.png"),
            FileSystem::getPath("resources/textures/skybox/left.png"),
            FileSystem::getPath("resources/textures/skybox/top.png"),
            FileSystem::getPath("resources/textures/skybox/bottom.png"),
            FileSystem::getPath("resources/textures/skybox/front.png"),
            FileSystem::getPath("resources/textures/skybox/back.png")
    };
    unsigned int cubemapTexture = loadCubemap(faces);

    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    // load models
    // -----------
    stbi_set_flip_vertically_on_load(false);

    Model forest("resources/objects/forest/forest.obj");
    forest.SetShaderTextureNamePrefix("material.");

    Model leaves("resources/objects/leaves/leaves.obj");
    leaves.SetShaderTextureNamePrefix("material.");

    Model bushes("resources/objects/bushes/bushes.obj");
    bushes.SetShaderTextureNamePrefix("material.");

    Model shrek("resources/objects/shrek/shrek.obj");
    shrek.SetShaderTextureNamePrefix(".material");

    Model vbuck1("resources/objects/vbuck/vbuck.obj");
    vbuck1.SetShaderTextureNamePrefix(".material");
    Model vbuck2("resources/objects/vbuck/vbuck.obj");
    vbuck2.SetShaderTextureNamePrefix(".material");
    Model vbuck3("resources/objects/vbuck/vbuck.obj");
    vbuck3.SetShaderTextureNamePrefix(".material");
    Model vbuck4("resources/objects/vbuck/vbuck.obj");
    vbuck4.SetShaderTextureNamePrefix(".material");
    Model vbuck5("resources/objects/vbuck/vbuck.obj");
    vbuck5.SetShaderTextureNamePrefix(".material");

    vector<glm::vec3> vbuckPositions;
    for(int i=0; i<NR_LIGHTS; i++) {
        float rngX = (rand() % 81) - 40;
        float rngZ = (rand() % 81) - 40;
        vbuckPositions.push_back(glm::vec3(rngX, 1.5f, rngZ));
    }

    PointLight pointLights[NR_LIGHTS];
    pointLights[0] = programState->pointLight;
    pointLights[0].position = programState->camera.Position;
    pointLights[0].ambient = glm::vec3(0.5, 0.3, 0.3);
    pointLights[0].diffuse = glm::vec3(0.5, 0.3, 0.3);
    pointLights[0].specular = glm::vec3(0.5, 0.2, 0.2);

    pointLights[0].constant = 1.0f;
    pointLights[0].linear = 0.48f;
    pointLights[0].quadratic = 0.48f;
    for(int i=1; i<NR_LIGHTS; i++) {
        pointLights[i].position = vbuckPositions[i];
        pointLights[i].ambient = glm::vec3(0.5, 0.5, 0.5);
        pointLights[i].diffuse = glm::vec3(0.5, 0.5, 0.5);
        pointLights[i].specular = glm::vec3(0.5, 0.5, 0.5);

        pointLights[i].constant = 1.0f;
        pointLights[i].linear = 0.48f;
        pointLights[i].quadratic = 0.48f;
    }

    // draw in wireframe
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // constants for light flickering
    int lightOffFrameCount = 0;
    int flickerOccurrenceFrequency = 16;
    int flickerFrequency = 1;

    // render loop
    // -----------
    // light position for flicker effect
    // shrek model whereabouts
    float curPosX = 0.0f;
    float curPosZ = 0.0f;
    while (!glfwWindowShouldClose(window)) {
        bool shouldDiscard = false;
        // per-frame time logic
        // --------------------
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Is current frame count divisible by frequency?
        int lightOffCond = (int(currentFrame) % flickerOccurrenceFrequency == 0);

        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(programState->clearColor.r, programState->clearColor.g, programState->clearColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // create depth cubemap transformation matrices
        float near_plane = 1.0f;
        float far_plane = 25.0f;
        glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), (float)SHADOW_WIDTH / (float)SHADOW_HEIGHT, near_plane, far_plane);
        std::vector<glm::mat4> shadowTransforms;
        shadowTransforms.push_back(shadowProj * glm::lookAt(pointLights[0].position, pointLights[0].position + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(pointLights[0].position, pointLights[0].position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(pointLights[0].position, pointLights[0].position + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(pointLights[0].position, pointLights[0].position + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(pointLights[0].position, pointLights[0].position + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(pointLights[0].position, pointLights[0].position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));

        // render scene to depth cubemap
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        depthShader.use();
        for (unsigned int i = 0; i < 6; ++i)
            depthShader.setMat4("shadowMatrices[" + std::to_string(i) + "]", shadowTransforms[i]);
        depthShader.setFloat("far_plane", far_plane);
        depthShader.setVec3("lightPos", pointLights[0].position);

        // forest model
        glm::mat4 forest_model = glm::mat4(1.0f);
        forest_model = glm::scale(forest_model, glm::vec3(8.0f, 8.0f, 8.0f));
        forest_model = glm::translate(forest_model, glm::vec3(0.0f, 0.0f, 0.0f));
        ourShader.setMat4("model", forest_model);
        forest.Draw(ourShader);

        // leaves model
        glCullFace(GL_FRONT);
        glm::mat4 leaves_model = glm::mat4(1.0f);
        leaves_model = glm::scale(leaves_model, glm::vec3(8.0f, 8.0f, 8.0f));
        leaves_model = glm::translate(leaves_model, glm::vec3(0.0f, 0.0f, 0.0f));
        ourShader.setMat4("model", leaves_model);
        leaves.Draw(ourShader);
        glCullFace(GL_BACK);

        // bushes model
        glDisable(GL_CULL_FACE);
        glm::mat4 bushes_model = glm::mat4(1.0f);
        bushes_model = glm::scale(bushes_model, glm::vec3(8.0f, 8.0f, 8.0f));
        bushes_model = glm::translate(bushes_model, glm::vec3(0.0f, 0.0f, 0.0f));
        ourShader.setMat4("model", bushes_model);
        bushes.Draw(ourShader);
        glEnable(GL_CULL_FACE);

        // shrek model
        if(lightOffCond && lightOffFrameCount < flickerFrequency) {
            shouldDiscard = true;
        }
        ourShader.setBool("shouldDiscard", shouldDiscard);
        glm::mat4 shrek_model = glm::mat4(1.0f);

        float camX = programState->camera.Position.x;
        float camZ = programState->camera.Position.z;
        float tmp1 = camX - curPosX;
        float tmp2 = camZ - curPosZ;
        float distance = sqrt(tmp1 * tmp1 + tmp2 * tmp2);
        if(distance >= 12.0f|| distance <= 5.0f) {
            curPosX = (float)(rand() % 25 - 12) + camX;
            curPosZ = (float)(rand() % 25 - 12) + camZ;
        }
        shrek_model = glm::inverse(glm::lookAt(glm::vec3(curPosX, 0.1f, curPosZ), programState->camera.Position, glm::vec3(0.0f, 1.0f, 0.0f)));
        shrek_model = glm::scale(shrek_model, glm::vec3(2.8f, 2.8f, 2.8f));
        shrek_model = glm::rotate(shrek_model, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, -0.2f));
        // a **very ugly** way to get a random-looking 'teleportation'
        if(lightOffFrameCount >= flickerFrequency) {
            float rng1 = (float)(rand() % 61 - 30);
            float rng2 = (float)(rand() % 61 - 30);
            float rng3 = (float)(rand() % 61 - 30);
            shrek_model = glm::inverse(glm::lookAt(glm::vec3(curPosX + rng1/30, 0.1f + rng2/90, curPosZ + rng3/30), programState->camera.Position, glm::vec3(0.0f, 1.0f, 0.0f)));
            shrek_model = glm::rotate(shrek_model, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, -0.2f));
            shrek_model = glm::scale(shrek_model, glm::vec3(2.8f, 2.8f, 2.8f));
            shrek_model = glm::rotate(shrek_model, glm::radians((float)rng1), glm::vec3(0.25f, 0, 0));
            shrek_model = glm::rotate(shrek_model, glm::radians((float)rng2), glm::vec3(0, 1.0f, 0));
            shrek_model = glm::rotate(shrek_model, glm::radians((float)rng3), glm::vec3(0, 0, 0.25f));
        }
        ourShader.setMat4("model", shrek_model);
        shrek.Draw(ourShader);

        shouldDiscard = false;
        ourShader.setBool("shouldDiscard", shouldDiscard);

        // vbuck models
        glm::mat4 vbuck_model1 = glm::mat4(1.0f);
        vbuck_model1 = glm::translate(vbuck_model1, vbuckPositions[0]);
        vbuck_model1 = glm::scale(vbuck_model1, glm::vec3(0.1f, 0.1f, 0.1f));
        vbuck_model1 = glm::rotate(vbuck_model1, glm::radians(125*currentFrame), glm::vec3(0, 1.0f, 0));
        ourShader.setMat4("model", vbuck_model1);
        vbuck1.Draw(ourShader);
        glm::mat4 vbuck_model2 = glm::mat4(1.0f);
        vbuck_model2 = glm::translate(vbuck_model2, vbuckPositions[1]);
        vbuck_model2 = glm::scale(vbuck_model2, glm::vec3(0.1f, 0.1f, 0.1f));
        vbuck_model2 = glm::rotate(vbuck_model2, glm::radians(125*currentFrame), glm::vec3(0, 1.0f, 0));
        ourShader.setMat4("model", vbuck_model2);
        vbuck2.Draw(ourShader);
        glm::mat4 vbuck_model3 = glm::mat4(1.0f);
        vbuck_model3 = glm::translate(vbuck_model3, vbuckPositions[2]);
        vbuck_model3 = glm::scale(vbuck_model3, glm::vec3(0.1f, 0.1f, 0.1f));
        vbuck_model3 = glm::rotate(vbuck_model3, glm::radians(125*currentFrame), glm::vec3(0, 1.0f, 0));
        ourShader.setMat4("model", vbuck_model3);
        vbuck3.Draw(ourShader);
        glm::mat4 vbuck_model4 = glm::mat4(1.0f);
        vbuck_model4 = glm::translate(vbuck_model4, vbuckPositions[3]);
        vbuck_model4 = glm::scale(vbuck_model4, glm::vec3(0.1f, 0.1f, 0.1f));
        vbuck_model4 = glm::rotate(vbuck_model4, glm::radians(125*currentFrame), glm::vec3(0, 1.0f, 0));
        ourShader.setMat4("model", vbuck_model4);
        vbuck4.Draw(ourShader);
        glm::mat4 vbuck_model5 = glm::mat4(1.0f);
        vbuck_model5 = glm::translate(vbuck_model5, vbuckPositions[4]);
        vbuck_model5 = glm::scale(vbuck_model5, glm::vec3(0.1f, 0.1f, 0.1f));
        vbuck_model5 = glm::rotate(vbuck_model5, glm::radians(125*currentFrame), glm::vec3(0, 1.0f, 0));
        ourShader.setMat4("model", vbuck_model5);
        vbuck5.Draw(ourShader);

        // If lightCond applies light is placed out of reach for this frame.
        // view/projection transformations
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        ourShader.use();
        pointLights[0].position = glm::vec3(curPosX, 4.5f + cos(currentFrame/4)/4, curPosZ);
        if(lightOffCond && lightOffFrameCount < flickerFrequency) {
            pointLights[0].position.y = -20.0f;
            lightOffFrameCount++;
        }
        else {
            pointLights[0].position.y = 4.5f + cos(currentFrame/4)/4;
            lightOffFrameCount = 0;
        }
        for(int i=0; i<NR_LIGHTS; i++) {
            vbuckPositions[i][1] = 1.5f + cos(currentFrame*2)/8;
            ourShader.setVec3("pointLights[" + std::to_string(i) + "].position", pointLights[i].position);
            ourShader.setVec3("pointLights[" + std::to_string(i) + "].ambient", pointLights[i].ambient);
            ourShader.setVec3("pointLights[" + std::to_string(i) + "].diffuse", pointLights[i].diffuse);
            ourShader.setVec3("pointLights[" + std::to_string(i) + "].specular", pointLights[i].specular);
            ourShader.setFloat("pointLights[" + std::to_string(i) + "].constant", pointLights[i].constant);
            ourShader.setFloat("pointLights[" + std::to_string(i) + "].linear", pointLights[i].linear);
            ourShader.setFloat("pointLights[" + std::to_string(i) + "].quadratic", pointLights[i].quadratic);
        }
        ourShader.setVec3("viewPosition", programState->camera.Position);
        ourShader.setFloat("material.shininess", 32.0f);

        glm::mat4 projection = glm::perspective(glm::radians(programState->camera.Zoom),
                                                (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = programState->camera.GetViewMatrix();

        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);
        ourShader.setInt("blinn", blinn);
        ourShader.setBool("shadows", shadows);
        ourShader.setFloat("far_plane", far_plane);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // forest model
        ourShader.setMat4("model", forest_model);
        forest.Draw(ourShader);

        // leaves model
        glCullFace(GL_FRONT);
        ourShader.setMat4("model", leaves_model);
        leaves.Draw(ourShader);
        glCullFace(GL_BACK);

        //bushes model
        glDisable(GL_CULL_FACE);
        ourShader.setMat4("model", bushes_model);
        bushes.Draw(ourShader);
        glEnable(GL_CULL_FACE);

        // shrek model
        if(lightOffCond && lightOffFrameCount < flickerFrequency) {
            shouldDiscard = true;
        }
        if(lightOffFrameCount >= flickerFrequency) {
            float rng1 = (float)(rand() % 61 - 30);
            float rng2 = (float)(rand() % 61 - 30);
            float rng3 = (float)(rand() % 61 - 30);
            shrek_model = glm::inverse(glm::lookAt(glm::vec3(curPosX + rng1/30, 0.1f + rng2/90, curPosZ + rng3/30), programState->camera.Position, glm::vec3(0.0f, 1.0f, 0.0f)));
            shrek_model = glm::rotate(shrek_model, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, -0.2f));
            shrek_model = glm::scale(shrek_model, glm::vec3(2.8f, 2.8f, 2.8f));
            shrek_model = glm::rotate(shrek_model, glm::radians((float)rng1), glm::vec3(0.25f, 0, 0));
            shrek_model = glm::rotate(shrek_model, glm::radians((float)rng2), glm::vec3(0, 1.0f, 0));
            shrek_model = glm::rotate(shrek_model, glm::radians((float)rng3), glm::vec3(0, 0, 0.25f));
        }
        ourShader.setBool("shouldDiscard", shouldDiscard);
        ourShader.setMat4("model", shrek_model);
        shrek.Draw(ourShader);
        shouldDiscard = false;
        ourShader.setBool("shouldDiscard", shouldDiscard);

        //vbuck model
        ourShader.setMat4("model", vbuck_model1);
        vbuck1.Draw(ourShader);
        ourShader.setMat4("model", vbuck_model2);
        vbuck2.Draw(ourShader);
        ourShader.setMat4("model", vbuck_model3);
        vbuck3.Draw(ourShader);
        ourShader.setMat4("model", vbuck_model4);
        vbuck4.Draw(ourShader);
        ourShader.setMat4("model", vbuck_model5);
        vbuck5.Draw(ourShader);

//        glBindFramebuffer(GL_FRAMEBUFFER, 0);
//        // 2. blur bright fragments with two-pass Gaussian Blur
//        // --------------------------------------------------
//        bool horizontal = true, first_iteration = true;
//        unsigned int amount = 10;
//        shaderBlur.use();
//        for (unsigned int i = 0; i < amount; i++)
//        {
//            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
//            shaderBlur.setInt("horizontal", horizontal);
//            glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);  // bind texture of other framebuffer (or scene if first iteration)
//            renderQuad();
//            horizontal = !horizontal;
//            if (first_iteration)
//                first_iteration = false;
//        }
//        glBindFramebuffer(GL_FRAMEBUFFER, 0);
//
//        // 3. now render floating point color buffer to 2D quad and tonemap HDR colors to default framebuffer's (clamped) color range
//        // --------------------------------------------------------------------------------------------------------------------------
//        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//        shaderBloomFinal.use();
//        glActiveTexture(GL_TEXTURE0);
//        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
//        glActiveTexture(GL_TEXTURE1);
//        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
//        shaderBloomFinal.setInt("bloom", bloom);
//        shaderBloomFinal.setFloat("exposure", exposure);
//        renderQuad();
//
//        std::cout << "bloom: " << (bloom ? "on" : "off") << "| exposure: " << exposure << std::endl;

        // skybox
        glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
        skyboxShader.use();
        view = glm::mat4(glm::mat3(programState->camera.GetViewMatrix())); // remove translation from the view matrix
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);
        // skybox cube
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS); // set depth function back to default

        if (programState->ImGuiEnabled)
            DrawImGui(programState);

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    programState->SaveToFile("resources/program_state.txt");
    delete programState;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);

    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(RIGHT, deltaTime);
    // blinn options
//    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS && !blinnKeyPressed)
//    {
//        blinn = !blinn;
//        blinnKeyPressed = true;
//    }
//    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_RELEASE)
//    {
//        blinnKeyPressed = false;
//    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !bloomKeyPressed)
    {
        bloom = !bloom;
        bloomKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE)
    {
        bloomKeyPressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    {
        if (exposure > 0.0f)
            exposure -= 0.001f;
        else
            exposure = 0.0f;
    }
    else if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    {
        exposure += 0.001f;
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (programState->CameraMouseMovementUpdateEnabled)
        programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    programState->camera.ProcessMouseScroll(yoffset);
}

void DrawImGui(ProgramState *programState) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();


    {
        static float f = 0.0f;
        ImGui::Begin("Hello window");
        ImGui::Text("Hello text");
        ImGui::SliderFloat("Float slider", &f, 0.0, 1.0);
        ImGui::ColorEdit3("Background color", (float *) &programState->clearColor);
        ImGui::DragFloat3("Backpack position", (float*)&programState->backpackPosition);
        ImGui::DragFloat("Backpack scale", &programState->backpackScale, 0.05, 0.1, 4.0);

        ImGui::DragFloat("pointLight.constant", &programState->pointLight.constant, 0.05, 0.0, 1.0);
        ImGui::DragFloat("pointLight.linear", &programState->pointLight.linear, 0.05, 0.0, 1.0);
        ImGui::DragFloat("pointLight.quadratic", &programState->pointLight.quadratic, 0.05, 0.0, 1.0);
        ImGui::End();
    }

    {
        ImGui::Begin("Camera info");
        const Camera& c = programState->camera;
        ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y, c.Position.z);
        ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
        ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y, c.Front.z);
        ImGui::Checkbox("Camera mouse update", &programState->CameraMouseMovementUpdateEnabled);
        ImGui::End();
    }

    glViewport(0, 0, 256, 256);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        programState->ImGuiEnabled = !programState->ImGuiEnabled;
        if (programState->ImGuiEnabled) {
            programState->CameraMouseMovementUpdateEnabled = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
}

unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 3);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
                // positions        // texture Coords
                -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}