#!/usr/bin/env bash
set -e

echo "[*] Installing system dependencies for gRPC + protobuf + C++..."

sudo apt update

sudo apt install -y \
    build-essential \
    autoconf \
    libtool \
    pkg-config \
    cmake \
    git \
    libprotobuf-dev \
    protobuf-compiler \
    libgrpc++-dev \
    libgrpc-dev

echo "[+] Checking versions:"
protoc --version
which grpc_cpp_plugin

echo "[+] Dependencies installed successfully."
echo "[+] You can now run: make"
