#!/usr/bin/env bash
# check_env.sh — month2 环境自检脚本
# 用法：bash scripts/check_env.sh
# 在 WSL2 Ubuntu 22.04 下运行

set -uo pipefail

PASS=0
FAIL=0

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

ok()   { echo -e "  ${GREEN}[✓]${NC} $1"; PASS=$((PASS + 1)); }
fail() { echo -e "  ${RED}[✗]${NC} $1"; FAIL=$((FAIL + 1)); }
info() { echo -e "  ${YELLOW}[i]${NC} $1"; }

echo ""
echo "============================================"
echo "  cpp-advanced-month2 环境检查"
echo "============================================"

# ── 1. MySQL 8.0 是否安装 ─────────────────────────────────────────────────────
echo ""
echo "【1】MySQL 8.0"
if command -v mysql &>/dev/null; then
    VER=$(mysql --version 2>/dev/null | grep -oP '\d+\.\d+\.\d+' | head -1)
    MAJOR=$(echo "$VER" | cut -d. -f1)
    if [[ "$MAJOR" -ge 8 ]]; then
        ok "mysql 已安装，版本 $VER"
    else
        fail "mysql 已安装，但版本 $VER < 8.0，请升级"
    fi
else
    fail "mysql 未安装 → sudo apt install mysql-server"
fi

# ── 2. MySQL 服务运行状态 ──────────────────────────────────────────────────────
echo ""
echo "【2】MySQL 服务 & chat_db 数据库"
if service mysql status &>/dev/null 2>&1; then
    ok "MySQL 服务正在运行"
else
    fail "MySQL 服务未运行 → sudo service mysql start"
fi

# 检查 chat_db 是否存在（无密码或有密码时均可用 --defaults-file 跳过）
if command -v mysql &>/dev/null && service mysql status &>/dev/null 2>&1; then
    if mysql -u root --connect-timeout=3 -e "USE chat_db;" &>/dev/null 2>&1; then
        ok "数据库 chat_db 存在"
    else
        # 尝试用 socket 认证
        if sudo mysql --connect-timeout=3 -e "USE chat_db;" &>/dev/null 2>&1; then
            ok "数据库 chat_db 存在（通过 sudo/socket 认证）"
        else
            fail "数据库 chat_db 不存在 → 执行: sudo mysql -e \"CREATE DATABASE chat_db;\""
        fi
    fi
else
    info "MySQL 服务未运行，跳过 chat_db 检查"
fi

# ── 3. libmysqlclient-dev ─────────────────────────────────────────────────────
echo ""
echo "【3】MySQL C++ 开发包"
if dpkg -s libmysqlclient-dev &>/dev/null 2>&1; then
    PKG_VER=$(dpkg -s libmysqlclient-dev 2>/dev/null | grep '^Version' | awk '{print $2}')
    ok "libmysqlclient-dev 已安装，版本 $PKG_VER"
elif dpkg -s libmariadb-dev &>/dev/null 2>&1; then
    PKG_VER=$(dpkg -s libmariadb-dev 2>/dev/null | grep '^Version' | awk '{print $2}')
    ok "libmariadb-dev 已安装（可替代），版本 $PKG_VER"
else
    fail "MySQL/MariaDB C++ 开发包未安装 → sudo apt install libmysqlclient-dev"
fi

# ── 4. Redis 安装 & 响应 PONG ─────────────────────────────────────────────────
echo ""
echo "【4】Redis"
if command -v redis-server &>/dev/null; then
    VER=$(redis-server --version 2>/dev/null | grep -oP 'v=\K[\d.]+' | head -1)
    ok "redis-server 已安装，版本 $VER"
else
    fail "redis-server 未安装 → sudo apt install redis-server"
fi

if command -v redis-cli &>/dev/null; then
    PONG=$(redis-cli --connect-timeout 2 ping 2>/dev/null || true)
    if [[ "$PONG" == "PONG" ]]; then
        ok "redis-cli ping → PONG（服务运行中）"
    else
        fail "redis-cli ping 未收到 PONG → sudo service redis-server start"
    fi
else
    fail "redis-cli 未找到"
fi

# ── 5. libhiredis-dev ─────────────────────────────────────────────────────────
echo ""
echo "【5】hiredis 开发包"
if dpkg -s libhiredis-dev &>/dev/null 2>&1; then
    PKG_VER=$(dpkg -s libhiredis-dev 2>/dev/null | grep '^Version' | awk '{print $2}')
    ok "libhiredis-dev 已安装，版本 $PKG_VER"
elif pkg-config --exists hiredis 2>/dev/null; then
    ok "hiredis 可通过 pkg-config 找到（手动编译安装）"
else
    fail "libhiredis-dev 未安装 → sudo apt install libhiredis-dev"
fi

# ── 汇总 ──────────────────────────────────────────────────────────────────────
echo ""
echo "============================================"
echo -e "  结果：${GREEN}${PASS} 项通过${NC} / ${RED}${FAIL} 项失败${NC}"
echo "============================================"
echo ""

if [[ $FAIL -eq 0 ]]; then
    echo -e "${GREEN}所有环境检查通过，可以开始 day1！${NC}"
    exit 0
else
    echo -e "${RED}有 ${FAIL} 项未通过，请按提示逐一修复后重跑此脚本。${NC}"
    exit 1
fi
