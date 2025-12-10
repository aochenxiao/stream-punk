# 流水账(StreamPunk) - 现代C++高性能序列化库
在当今数据驱动的应用中，高效序列化解决方案至关重要。
流水账专为现代C++设计，你可以原生态使用C++，直接操作对象进行序列化、反序列化，无需额外的包装。
因为它支持大部分C++标准库中的数据类型，它还支持各种指针数据。
总之，体验一下你就知道，你会用起来得心应手，如臂使指。

## 目前版本能力
目前是序列化与反序列化功能。
已实现与TS之间的序列化与反序列化，JS的实现依靠对TS的编译，所以也算支持JS。

## 快速入门指南
必须说明的是，目前C++26的标准还没有正式发布，所以StreamPunk目前只支持C++23的标准。
所以目前的版本是高水平使用宏实现静态元信息。
当C++26正式发布之后，StreamPunk会迁移到使用C++26的新特性实现静态元信息。

### 1. 包含头文件
```cpp
#include "StreamPunk.hpp"
```

### 2. 注册自定义类型（在customData.hpp）
```cpp
// customData.hpp
#define Xt_CustomType(X__) \
X__(Device, Device) \
X__(Sensor, Sensor) \
X__(SmartHomeSystem, SmartHomeSystem)
// 添加更多业务相关类型...
```

### 3. 创建可序列化类
```cpp
struct Device : public Base {
    #define Xt_Device(X__) \
    X__(std::string, deviceId, "unknown") \
    X__(std::string, manufacturer, "") \
    X__(std::chrono::system_clock::time_point, lastSeen, {})
    
    Device() = default;
    UseData(Device); // 关键宏声明
};
```

### 4. 核心API使用
```cpp
int main() {
    INIT_StreamPunk(); // 必须初始化
    
    // 序列化
    SmartHomeSystem homeSystem;
    std::stringstream binStream;
    O output{binStream};
    output << homeSystem;
    
    // 反序列化
    SmartHomeSystem restored;
    I input{binStream};
    input >> restored;
    
    // 深拷贝
    DeepCopier copier;
    SmartHomeSystem copy;
    deepCopy(copier, copy, homeSystem);
    copier.clear(); // 必须清理
}
```

## 使用规范

### 关键约束
1. **编译要求**：
   - C++20或更高标准
   - 必须启用RTTI
   - UTF-8无BOM编码源文件

2. **指针使用**：
   - 可使用裸指针成员, 仅推荐配合std::unique_ptr时使用.
   - 推荐组合：
     - std::shared_ptr + std::weak_ptr
     - std::unique_ptr + raw ptr
   - 使用void\*时请注意:序列化时,遇到void\*,必须在此之前,其指向的对象就已经被序列化过.

**重要限制**：当前版本仅支持相同端序架构（大端-大端或小端-小端）的机器间数据互通，不支持跨端序数据交换。

# 如何运行示例
安装个visual studio 2022以上的版本, 打开项目，进行如此配置：
