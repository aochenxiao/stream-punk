# pragma once
/*
自定义类型注册表
左边类型,可以写域操作符,以及特化模板类型.
右边类型名称,只能是id,并且不可重复.
*/
# define Xt_CustomType(X__) \
X__(AllBasicTypes, AllBasicTypes) \
X__(TemplateContainer, TemplateContainer) \
X__(PointerContainer, PointerContainer) \
X__(ComplexTemplateNesting, ComplexTemplateNesting) \
X__(ComprehensiveContainer, ComprehensiveContainer)  \
X__(Child, Child) \
X__(SelfReferential, SelfReferential) \
X__(TemplateAndPointer, TemplateAndPointer) \
X__(InheritanceAndSelfReference, InheritanceAndSelfReference) \
X__(MegaComplexClass, MegaComplexClass) \
X__(SuperComplexContainer, SuperComplexContainer) \
X__(Test , Test) \
X__(MQTT , MQTT) \
X__(PointerDemo, PointerDemo) \
X__(ContainerDemo, ContainerDemo) \
X__(NetworkSystem, NetworkSystem) \
X__(Device, Device) \
X__(NetworkDevice, NetworkDevice) \
X__(Sensor,Sensor) \
X__(TemperatureSensor,TemperatureSensor) \
X__(SmartHomeSystem,SmartHomeSystem) \
X__(MultiLevelContainer,MultiLevelContainer) \
X__(SptrTest, SptrTest) \
X__(MousePosition, MousePosition) \




