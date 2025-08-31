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
    in rec {
      devShells.default = pkgs.stdenv.mkDerivation {
        name = "build";
        buildInputs = [ pkgs.dbus pkgs.xorg.libX11 pkgs.xorg.libXScrnSaver ];
        nativeBuildInputs = [ pkgs.pkgconf pkgs.scdoc ];
      };
      packages.default = pkgs.stdenv.mkDerivation {
        name="unified-inhibit";
        src = ./.;
        buildInputs = [ pkgs.dbus pkgs.xorg.libX11 pkgs.xorg.libXScrnSaver ];
        nativeBuildInputs = [ pkgs.pkgconf ];
        doCheck = false;
        doConfigure = false;
        enableParallelBuilding = true;
        makeFlags = [ "prefix=$(out)" ];
        #checkPhase = ''
        #  mkdir -p test
        #  cp $src/test/dbus.conf ./test
        #  make test
        #'';
      };
      apps.default = { type = "app"; program = "${packages.default}/bin/uinhibitd"; };
    }
  );
}
