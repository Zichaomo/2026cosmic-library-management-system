@echo off
chcp 65001 >nul
title 图书馆管理系统
echo.
echo ========================================
echo    智慧图书馆管理系统 启动中...
echo ========================================
echo.

where node >nul 2>nul
if errorlevel 1 (
    echo [错误] 未检测到 Node.js，请先安装 Node.js 18 或更高版本
    echo 下载地址: https://nodejs.org/
    pause
    exit /b 1
)

echo Node 版本:
node -v
echo.
echo 启动服务，访问 http://127.0.0.1:3000
echo 按 Ctrl+C 停止
echo.

node server.js
if errorlevel 1 (
    echo.
    echo [错误] 启动失败，请检查上方日志
    pause
)
