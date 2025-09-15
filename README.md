# HTTPS Download Library

这是一个可移植的 HTTPS 下载库，从嵌入式系统代码移植到 Linux 环境。该库使用系统抽象层来实现跨平台兼容性。

## 特性

- 支持 HTTPS 协议下载
- 系统抽象层设计，易于移植到不同平台
- 错误重试机制
- 进度监控
- 内存安全管理
- 完整的测试套件

## 文件结构

```
├── system_abstraction.h          # 系统抽象层接口定义
├── system_abstraction_linux.c    # Linux 平台实现
├── https_download.h              # HTTPS 下载库接口
├── https_download.c              # HTTPS 下载库实现
├── download_tool.c               # 命令行下载工具
├── test_download.c               # 测试代码
├── build.sh                      # 构建脚本
├── Makefile                      # 编译配置
└── README.md                     # 说明文档
```

## 依赖

- mbedTLS 库 (libmbedtls-dev)
- GCC 编译器
- Linux 系统

## 编译和安装

### 1. 安装依赖

在 Ubuntu/Debian 系统上：

```bash
make install-deps
```

或手动安装：

```bash
sudo apt-get update
sudo apt-get install -y libmbedtls-dev build-essential
```

### 2. 检查依赖

```bash
make check-deps
```

### 3. 编译

```bash
# 编译所有目标
make

# 只编译库
make bin/libhttps_download.a

# 只编译测试程序
make bin/test_download

# 调试版本
make debug

# 发布版本
make release
```

### 4. 运行测试

```bash
# 运行所有测试
make test

# 或直接运行测试程序
./bin/test_download

# 跳过性能测试
./bin/test_download --no-performance

# 跳过 URL 测试
./bin/test_download --no-url-tests
```

### 5. 使用下载工具

```bash
# 基本用法
./bin/download https://httpbin.org/json

# 指定保存路径
./bin/download https://httpbin.org/json ./data.json

# 指定输出文件名
./bin/download -o myfile.json https://httpbin.org/json

# 显示详细信息
./bin/download -v https://httpbin.org/json

# 显示帮助
./bin/download --help
```

## 使用方法

### 基本用法

```c
#include "https_download.h"

int main()
{
    int result = https_download("https://example.com/file.zip", "./downloaded_file.zip");
    if (result == 0) {
        printf("Download successful!\n");
    } else {
        printf("Download failed with error: %d\n", result);
    }
    return 0;
}
```

### 链接库

编译时需要链接 mbedTLS 库：

```bash
gcc -o myapp myapp.c -L./bin -lhttps_download -lmbedtls -lmbedx509 -lmbedcrypto
```

## API 参考

### https_download

```c
int https_download(char *url, const char *save_path);
```

**参数：**
- `url`: 要下载的 HTTPS URL
- `save_path`: 保存文件的本地路径

**返回值：**
- `0`: 下载成功
- `负值`: 下载失败

**示例：**

```c
// 下载 JSON 文件
int result = https_download("https://httpbin.org/json", "./data.json");

// 下载二进制文件
int result = https_download("https://example.com/image.jpg", "./image.jpg");
```

## 系统抽象层

为了支持不同平台，本库实现了系统抽象层：

### 内存管理
- `sys_malloc()` - 内存分配
- `sys_calloc()` - 零初始化内存分配
- `sys_free()` - 内存释放

### 随机数生成
- `sys_get_random_bytes()` - 生成随机字节

### 时间和延迟
- `sys_delay_ms()` - 毫秒级延迟

### 日志记录
- `SYS_LOG_INFO()` - 信息日志
- `SYS_LOG_ERROR()` - 错误日志
- `SYS_LOG_DEBUG()` - 调试日志

### 文件系统
- `sys_file_open()` - 打开文件
- `sys_file_write()` - 写入文件
- `sys_file_close()` - 关闭文件

## 移植到其他平台

要移植到其他平台，只需要：

1. 实现 `system_abstraction.h` 中定义的接口
2. 创建对应平台的实现文件（如 `system_abstraction_windows.c`）
3. 修改 Makefile 以支持新平台

## 测试

测试套件包含以下测试：

1. **系统抽象层测试** - 验证所有抽象接口正常工作
2. **HTTPS 下载测试** - 测试下载不同大小的文件
3. **错误处理测试** - 测试无效 URL 和路径的处理
4. **性能测试** - 测量下载速度和性能
5. **URL 解析测试** - 测试各种 URL 格式

## 故障排除

### 编译错误

如果遇到 mbedTLS 相关的编译错误：

```bash
# 确保安装了开发包
sudo apt-get install libmbedtls-dev

# 检查安装
pkg-config --cflags --libs mbedtls
```

### 运行时错误

1. **网络连接问题**：确保系统可以访问互联网
2. **权限问题**：确保有写入目标目录的权限
3. **SSL/TLS 错误**：某些服务器可能需要特定的 SSL 配置

### 调试

启用调试模式编译：

```bash
make debug
```

这将启用详细的日志输出来帮助诊断问题。

## 许可证

本项目基于原始代码移植，请遵循相应的许可证要求。

## 贡献

欢迎提交 Issue 和 Pull Request 来改进这个库。
