#pragma once
// 注册 demo 类型到 StreamPunk 类型系统
// 此文件在 #include <stream-punk/StreamPunk.hpp> 之前被包含，
// 以定义 Xt_CustomType 宏，使 StreamPunk 能识别这些自定义类型。

// 前向声明（类型定义在 Data.hpp 中）
namespace sp {
struct AllBasicTypes;
struct TemplateContainer;
struct ComplexTemplateNesting;
struct ComprehensiveContainer;
struct PointerContainer;
struct Child;
struct SelfReferential;
struct TemplateAndPointer;
struct InheritanceAndSelfReference;
struct MegaComplexClass;
struct SuperComplexContainer;
struct Test;
struct MQTT;
struct PointerDemo;
struct ContainerDemo;
struct NetworkSystem;
struct Device;
struct NetworkDevice;
struct Sensor;
struct TemperatureSensor;
struct SmartHomeSystem;
struct MultiLevelContainer;
struct SptrTest;
struct MousePosition;
struct ShadowTestData;
}

// Xt_CustomType_DEMO 提供 demo 类型的注册基础
// 测试代码可以基于此追加测试专用类型
#define Xt_CustomType_DEMO(X__) \
X__(sp::AllBasicTypes, AllBasicTypes) \
X__(sp::TemplateContainer, TemplateContainer) \
X__(sp::ComplexTemplateNesting, ComplexTemplateNesting) \
X__(sp::ComprehensiveContainer, ComprehensiveContainer) \
X__(sp::PointerContainer, PointerContainer) \
X__(sp::Child, Child) \
X__(sp::SelfReferential, SelfReferential) \
X__(sp::TemplateAndPointer, TemplateAndPointer) \
X__(sp::InheritanceAndSelfReference, InheritanceAndSelfReference) \
X__(sp::MegaComplexClass, MegaComplexClass) \
X__(sp::SuperComplexContainer, SuperComplexContainer) \
X__(sp::Test, Test) \
X__(sp::MQTT, MQTT) \
X__(sp::PointerDemo, PointerDemo) \
X__(sp::ContainerDemo, ContainerDemo) \
X__(sp::NetworkSystem, NetworkSystem) \
X__(sp::Device, Device) \
X__(sp::NetworkDevice, NetworkDevice) \
X__(sp::Sensor, Sensor) \
X__(sp::TemperatureSensor, TemperatureSensor) \
X__(sp::SmartHomeSystem, SmartHomeSystem) \
X__(sp::MultiLevelContainer, MultiLevelContainer) \
X__(sp::SptrTest, SptrTest) \
X__(sp::MousePosition, MousePosition) \
X__(sp::ShadowTestData, ShadowTestData)

// 默认：Xt_CustomType = Xt_CustomType_DEMO
// 测试代码可通过创建 test/custom_type.hpp 覆盖此定义来追加测试类型
#define Xt_CustomType Xt_CustomType_DEMO