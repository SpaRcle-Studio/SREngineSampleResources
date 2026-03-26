//
// Created by innerviewer on 2026-03-20.
//

#include <Tests/Networking/Networking.h>

#include <Codegen/Networking.generated.hpp>

namespace Samples {
    void Networking::Start() {
        SR_TRACY_ZONE;
    
        m_lastSentPosition = gameObject->GetTransform()->GetTranslation();
        m_networkManager = gameObject->GetComponent<SR_CORE_NS::NetworkManager>();
        if (!m_networkManager) {
            SR_ERROR("Networking::Start() : NetworkManager component not found on this GameObject!");
            return;
        }

        /// Set up message received callback
        m_networkManager->SetOnMessageReceived([this](const SR_CORE_NS::NetworkMessage& msg) {
            /// This callback fires when a message arrives.
            /// We can process immediately or let Update() poll via PopMessage().
            /// For this sample, we'll poll in Update().
        });
    }

    void Networking::Update(float_t dt) {
        SR_TRACY_ZONE;

        if (!m_networkManager) {
            SR_ERROR("Networking::Update() : No NetworkManager available!");
            return; /// No NetworkManager, can't do networking
        }

        if (!m_isInitialized) {
            if (isHost) {
                SR_LOG("Networking::Start() : Hosting on port " + std::to_string(port));
                m_networkManager->SetAddress(hostAddress);
                
                m_networkManager->HostSession();
            } else {
                SR_LOG("Networking::Start() : Joining " + hostAddress + ":" + std::to_string(port));
                m_networkManager->JoinSession(hostAddress, port);
            }
        }

        /// Process incoming position updates from peers
        ProcessIncomingMessages();

        /// Send our position at the configured interval
        m_timeSinceLastSync += dt;
        if (m_timeSinceLastSync >= syncInterval && m_lastSentPosition != gameObject->GetTransform()->GetTranslation()) {
            m_timeSinceLastSync = 0.0f;
            SendPositionUpdate();
            m_lastSentPosition = gameObject->GetTransform()->GetTranslation();
        }
    }

    void Networking::OnDestroy() {
        if (m_networkManager) {
            m_networkManager->Disconnect();
        }
        Super::OnDestroy();
    }

    void Networking::ProcessIncomingMessages() {
        /// Pop all pending messages and process them
        auto messages = m_networkManager->PopAllMessages();
        for (const auto& msg : messages) {
            /// Expecting position data: 3 floats (x, y, z) = 12 bytes
            if (msg.data.size() >= sizeof(float) * 3) {
                float x, y, z;
                std::memcpy(&x, msg.data.data(), sizeof(float));
                std::memcpy(&y, msg.data.data() + sizeof(float), sizeof(float));
                std::memcpy(&z, msg.data.data() + sizeof(float) * 2, sizeof(float));

                SR_LOG("Networking: Received position from peer " + std::to_string(msg.peerId) +
                       ": (" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")");

                /// In a real game, you would find the remote player's GameObject
                /// and update its transform. For this sample, we just log it.
                /// Example:
                /// auto* remotePlayer = FindRemotePlayer(msg.peerId);
                /// if (remotePlayer) {
                ///     remotePlayer->GetTransform()->SetTranslation(SR_MATH_NS::FVector3(x, y, z));
                /// }
            }
        }
    }

    void Networking::SendPositionUpdate() {
        auto&& transform = gameObject->GetTransform();
        if (!transform) {
            return;
        }

        auto pos = transform->GetTranslation();

        /// Pack position into bytes: 3 floats
        std::vector<uint8_t> data(sizeof(float) * 3);
        float x = pos.x;
        float y = pos.y;
       float z = pos.z;
        std::memcpy(data.data(), &x, sizeof(float));
        std::memcpy(data.data() + sizeof(float), &y, sizeof(float));
        std::memcpy(data.data() + sizeof(float) * 2, &z, sizeof(float));

        /// Broadcast our position to all connected peers
        m_networkManager->Broadcast(data.data(), data.size());
    }
}
