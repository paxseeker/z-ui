{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    meson
    pkg-config
    wayland
    wayland-scanner
  ];
}
