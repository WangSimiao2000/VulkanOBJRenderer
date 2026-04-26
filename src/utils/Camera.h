#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum CameraDirection { FORWARD, BACKWARD, LEFT, RIGHT };

class Camera {
public:
    glm::vec3 position{0.0f, 1.0f, 3.0f};
    glm::vec3 front{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};

    float yaw   = -90.0f;
    float pitch  = 0.0f;
    float speed  = 2.5f;
    float sensitivity = 0.1f;

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

    void processKeyboard(CameraDirection dir, float dt) {
        float v = speed * dt;
        switch (dir) {
            case FORWARD:  position += front * v; break;
            case BACKWARD: position -= front * v; break;
            case LEFT:     position -= right * v; break;
            case RIGHT:    position += right * v; break;
        }
    }

    void processMouseMovement(float xoff, float yoff) {
        xoff *= sensitivity;
        yoff *= sensitivity;
        yaw   += xoff;
        pitch += yoff;
        if (pitch >  89.0f) pitch =  89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        updateVectors();
    }

private:
    void updateVectors() {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(f);
        right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
        up    = glm::normalize(glm::cross(right, front));
    }
};
