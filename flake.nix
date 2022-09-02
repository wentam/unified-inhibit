{
  inputs = {
    nixpkgs.url = github:NixOS/nixpkgs/nixos-unstable;
    flake-utils.url = "github:numtide/flake-utils";
    wentampkgs.url = git+ssh://wentam@wentam.net:/mnt/NAS/git-host/nix-pkgs.git;
    wentampkgs.flake = false;
  };

  outputs = {self, nixpkgs, flake-utils, wentampkgs, ...}:
  flake-utils.lib.eachDefaultSystem (
    system:
    let
      pkgs = nixpkgs.legacyPackages.${system};
      wentam = import wentampkgs { inherit pkgs; };
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
