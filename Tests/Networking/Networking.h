//
// Created by innerviewer on 2026-03-20.
//

#ifndef SR_ENGINE_SAMPLES_TESTS_NETWORKING_H
#define SR_ENGINE_SAMPLES_TESTS_NETWORKING_H

#include <Scripting/Cpp/CppBehaviour.h>
#include <Engine/Network/NetworkManager.h>

#include <Utils/ECS/GameObject.h>
#include <Utils/ECS/Transform3D.h>
#include <Utils/Types/String.h>

namespace Samples {
    /// @category(Game)
    /// A sample script demonstrating P2P networking.
    /// Attach this to a GameObject along with a NetworkManager component.
    /// Set isHost=true on one instance and isHost=false on others.
    class Networking : public SpaRcle::Scripting::CppBehaviour {
        SR_CLASS()
        using Super = CppBehaviour;
    public:
        void Start() override;
        void Update(float_t dt) override;
        void OnDestroy() override;

    public:
        /// @property
        /// @category(Network)
        bool isHost = false;

        /// @property
        /// @category(Network)
        SR_UTILS_NS::String hostAddress = "127.0.0.1";

        /// @property
        /// @category(Network)
        uint16_t port = 7777;

        /// @property
        /// @category(Network)
        float_t syncInterval = 0.05f; /// Send position every 50ms

    private:
        void ProcessIncomingMessages();
        void SendPositionUpdate();

    private:
        bool m_isInitialized = false;
        float_t m_timeSinceLastSync = 0.0f;
        SR_MATH_NS::FVector3 m_lastSentPosition;
        SR_HTYPES_NS::SharedPtr<SR_CORE_NS::NetworkManager> m_networkManager;
    };
} // Samples

#endif //SR_ENGINE_SAMPLES_TESTS_NETWORKING_H