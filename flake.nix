{
  inputs = {
    nixpkgs.url = github:NixOS/nixpkgs/nixos-unstable;
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {self, nixpkgs, flake-utils, ...}:
  flake-utils.lib.eachDefaultSystem (
    system:
    let
      pkgs = nixpkgs.legacyPackages.${system};
    in {
      devShells.default = pkgs.gcc12Stdenv.mkDerivation {
        name = "build";
        buildInputs = [ pkgs.dbus ];
        NIX_CFLAGS_COMPILE="-isystem ${pkgs.dbus.dev}/include/dbus-1.0/ -isystem ${pkgs.dbus.lib}/lib/dbus-1.0/include/";
      };
      packages.default = pkgs.stdenv.mkDerivation {
        name="unified-inhibit";
        src = ./.;
        buildInputs = [ pkgs.dbus ];
        NIX_CFLAGS_COMPILE="-isystem ${pkgs.dbus.dev}/include/dbus-1.0/ -isystem ${pkgs.dbus.lib}/lib/dbus-1.0/include/";
        installPhase = ''
          mkdir -p $out/bin
          cp build/uinhibitd $out/bin
        '';
      };
    }
  );
}
