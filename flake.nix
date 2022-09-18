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
      devShells.default = pkgs.gcc12Stdenv.mkDerivation {
        name = "build";
        buildInputs = [ pkgs.dbus ];
        nativeBuildInputs = [ pkgs.pkgconf ];
      };
      packages.default = pkgs.stdenv.mkDerivation {
        name="unified-inhibit";
        src = ./.;
        buildInputs = [ pkgs.dbus ];
        nativeBuildInputs = [ pkgs.pkgconf ];
        doCheck = true;
        buildPhase = ''make -j12 prefix=$out'';
        installPhase = ''make install prefix=$out'';
        checkPhase = ''
          mkdir -p test
          cp $src/test/dbus.conf ./test
          make test
        '';
      };
      apps.default = { type = "app"; program = "${packages.default}/bin/uinhibitd"; };
    }
  );
}
