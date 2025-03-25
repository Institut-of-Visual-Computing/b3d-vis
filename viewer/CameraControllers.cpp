#include "CameraControllers.h"

//#include "ImGuizmo.h"
#include "glm/ext/matrix_transform.hpp"
#include "imgui.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/component_wise.hpp>


auto FirstPersonCameraController::update() -> void
{
	const auto viewportIsFocused = ImGui::IsItemFocused();
	auto& io = ImGui::GetIO();

	constexpr auto fastSpeed = 25.0f;
	float rollAngle = 0.0f;
	auto cameraMoveAcceleration = glm::vec3{ 0 };
	if (viewportIsFocused)
	{
		if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
		{
			moveCameraFaster_ = true;
		}

		if (ImGui::IsKeyReleased(ImGuiKey_LeftShift))
		{
			moveCameraFaster_ = false;
		}

		if (ImGui::IsKeyDown(ImGuiKey_W))
		{
			cameraMoveAcceleration =
				camera_->forward_ * camera_->movementSpeedScale_ * (moveCameraFaster_ ? fastSpeed : 1.0f);
		}
		if (ImGui::IsKeyDown(ImGuiKey_S))
		{
			cameraMoveAcceleration =
				-camera_->forward_ * camera_->movementSpeedScale_ * (moveCameraFaster_ ? fastSpeed : 1.0f);
		}
		if (ImGui::IsKeyDown(ImGuiKey_A))
		{
			cameraMoveAcceleration = -glm::normalize(glm::cross(camera_->forward_, camera_->getUp())) *
				camera_->movementSpeedScale_ * (moveCameraFaster_ ? fastSpeed : 1.0f);
		}
		if (ImGui::IsKeyDown(ImGuiKey_D))
		{
			cameraMoveAcceleration = glm::normalize(glm::cross(camera_->forward_, camera_->getUp())) *
				camera_->movementSpeedScale_ * (moveCameraFaster_ ? fastSpeed : 1.0f);
		}

		// Roll-Rotation mit den Tasten "Q" und "E"
		constexpr auto rollSensitivity = 0.1f; // Empfindlichkeit der Roll-Rotation

		if (ImGui::IsKeyDown(ImGuiKey_Q)) // Roll nach links (Q)
		{
			rollAngle = glm::radians(rollSensitivity * io.DeltaTime);
			const auto rollRotation = glm::angleAxis(rollAngle, camera_->forward_);

			// Lokalen Up-Vektor und Right-Vektor aktualisieren
			camera_->up_ = glm::normalize(rollRotation * camera_->up_);
			camera_->right_ = glm::normalize(rollRotation * camera_->right_);
		}
		else if (ImGui::IsKeyDown(ImGuiKey_E)) // Roll nach rechts (E)
		{
			rollAngle = glm::radians(-rollSensitivity * io.DeltaTime);
			const auto rollRotation = glm::angleAxis(rollAngle, camera_->forward_);

			// Lokalen Up-Vektor und Right-Vektor aktualisieren
			camera_->up_ = glm::normalize(rollRotation * camera_->up_);
			camera_->right_ = glm::normalize(rollRotation * camera_->right_);
		}



	}

	auto delta = io.MouseDelta;
	delta.x *= -1.0;

	if (ImGui::IsItemHovered())
	{
		const auto wheelValue = io.MouseWheel;
		if (wheelValue != 0.0f)
		{
			cameraMoveAcceleration = camera_->forward_ * camera_->movementSpeedScale_ * wheelValue * fastSpeed;
		}
	}

	if (ImGui::IsItemActive())
	{
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
		{
			const auto right = glm::normalize(glm::cross(camera_->forward_, camera_->getUp()));
			cameraMoveAcceleration += -glm::normalize(glm::cross(camera_->forward_, right)) *
				camera_->movementSpeedScale_ * (moveCameraFaster_ ? fastSpeed : 1.0f) * delta.y;

			cameraMoveAcceleration +=
				right * camera_->movementSpeedScale_ * (moveCameraFaster_ ? fastSpeed : 1.0f) * delta.x;
		}
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			constexpr auto sensitivity = 0.1f; // Empfindlichkeit der Mausbewegung
			// Lokaler "Up"-Vektor der Kamera
			auto up = camera_->up_; // Verwende den lokalen Up-Vektor der Kamera

			// Überprüfen, ob forward_ fast parallel zum globalen Up-Vektor ist
			//const auto dotProduct = glm::dot(camera_->forward_, up);
			//const auto isParallel = glm::abs(dotProduct) > 0.99f; // Schwellenwert für Parallelität

			glm::vec3 right;
			right = glm::normalize(glm::cross(up, camera_->forward_)); // Normale Berechnung

			// Berechnung der Rotationswinkel für horizontale und vertikale Bewegung
			const auto horizontalAngle =
				glm::radians(-delta.x * sensitivity); // Horizontale Rotation (um die "Up"-Achse)
			const auto verticalAngle =
				glm::radians(-delta.y * sensitivity); // Vertikale Rotation (um die "Right"-Achse)

			// Horizontale Rotation (um die lokale "Up"-Achse)
			const auto horizontalRotation = glm::angleAxis(horizontalAngle, up);

			// Vertikale Rotation (um die lokale "Right"-Achse)
			const auto verticalRotation = glm::angleAxis(verticalAngle, right);

			// Kombinierte Rotation
			const auto rotationQuat = horizontalRotation * verticalRotation;

			// Neue Blickrichtung berechnen
			camera_->forward_ = glm::normalize(rotationQuat * camera_->forward_);

			// Vektoren aktualisieren
			camera_->right_ = glm::normalize(glm::cross(camera_->forward_, up));
			camera_->up_ = glm::normalize(glm::cross(camera_->right_, camera_->forward_));
		}
	}

	camera_->position_ += cameraMoveAcceleration * io.DeltaTime;
}


auto OrbitCameraController::update() -> void
{
	if (!camera_)
		return; // Überprüfe, ob die Kamera gültig ist

	auto& io = ImGui::GetIO();
	const auto viewportIsFocused = ImGui::IsItemFocused();
	auto cameraMoveAcceleration = glm::vec3{ 0 };
	float rollAngle = 0.0f;

	constexpr auto focusPosition = glm::vec3{ 0.0f, 0.0f, 0.0f };

	auto delta = io.MouseDelta;
	delta.x *= -1.0; // Invertiere die horizontale Mausbewegung (falls gewünscht)

	if (ImGui::IsItemHovered() && std::isfinite(io.MouseWheel))
	{
		const auto wheelValue = io.MouseWheel;
		if (wheelValue != 0.0f)
		{
			constexpr auto fastSpeed = 25.0f;
			cameraMoveAcceleration = camera_->forward_ * camera_->movementSpeedScale_ * wheelValue * fastSpeed;
		}
	}

	if (viewportIsFocused)
	{
		// Roll-Rotation mit den Tasten "Q" und "E"
		constexpr auto rollSensitivity = 0.2f; // Empfindlichkeit der Roll-Rotation

		if (ImGui::IsKeyDown(ImGuiKey_Q)) // Roll nach links (Q)
		{
			rollAngle = glm::radians(-rollSensitivity);
			const auto rollRotation = glm::angleAxis(rollAngle, camera_->forward_);

			// Lokalen Up-Vektor und Right-Vektor aktualisieren
			camera_->up_ = glm::normalize(rollRotation * camera_->up_);
			camera_->right_ = glm::normalize(rollRotation * camera_->right_);
		}
		else if (ImGui::IsKeyDown(ImGuiKey_E)) // Roll nach rechts (E)
		{
			rollAngle = glm::radians(rollSensitivity);
			const auto rollRotation = glm::angleAxis(rollAngle, camera_->forward_);

			// Lokalen Up-Vektor und Right-Vektor aktualisieren
			camera_->up_ = glm::normalize(rollRotation * camera_->up_);
			camera_->right_ = glm::normalize(rollRotation * camera_->right_);
		}
	}

	if (ImGui::IsItemActive())
	{
		// Mausgesteuerte horizontale und vertikale Rotation
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
		{
			constexpr auto sensitivity = 0.005f; // Empfindlichkeit der Mausbewegung

			// Mausbewegung in Pixel
			auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);

			// Umwandlung in relativierte Werte zwischen -1 und 1
			auto normalizedX = delta.x * sensitivity;
			auto normalizedY = delta.y * sensitivity;

			// Berechne die Rotationsachse und den Winkel für die horizontale und vertikale Drehung
			// Horizontale Rotation (um die Y-Achse)
			glm::vec3 axisOfRotationHorizontal = glm::cross(camera_->forward_, camera_->right_);
			float angleHorizontal = -normalizedX; // Horizontal (um die Y-Achse)

			// Vertikale Rotation (um die X-Achse)
			glm::vec3 axisOfRotationVertical = camera_->right_;
			float angleVertical = normalizedY; // Vertikal (um die X-Achse)

			// Wende die horizontale Rotation an
			if (angleHorizontal != 0.0f)
			{
				glm::quat rotationQuatHorizontal =
					glm::angleAxis(angleHorizontal, glm::normalize(axisOfRotationHorizontal));
				camera_->forward_ = glm::normalize(rotationQuatHorizontal * camera_->forward_);
			}

			// Wende die vertikale Rotation an
			if (angleVertical != 0.0f)
			{
				glm::quat rotationQuatVertical = glm::angleAxis(angleVertical, glm::normalize(axisOfRotationVertical));
				camera_->forward_ = glm::normalize(rotationQuatVertical * camera_->forward_);
			}

			// Vektoren für Right und Up neu berechnen, um die Kamera-Ausrichtung zu aktualisieren
			camera_->right_ = glm::normalize(glm::cross(camera_->forward_, camera_->up_));
			camera_->up_ = glm::normalize(glm::cross(camera_->right_, camera_->forward_));

			// Kameraposition aktualisieren (um den Fokuspunkt rotieren)
			const auto focusDistance = glm::max(glm::distance(focusPosition, camera_->position_), 0.001f);
			camera_->position_ = focusPosition - camera_->forward_ * focusDistance;

			// Drag-Delta zurücksetzen, um die Mausbewegung zu "bestätigen"
			ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
		}
	}





	camera_->position_ += cameraMoveAcceleration * io.DeltaTime;
}
