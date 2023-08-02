{
  description = "DeviceTree Compiler v1.8.0 - Fork by MtM";

  inputs = {
    # what nixos version is this?
    # nixpkgs.url = "github:nixos/nixpkgs/nixos-xx.xx";

    dtc.url = "github:MaxTheMooshroom/dtc/dev";
  };

  outputs = { self, nixpkgs, dtc }: {
    packages.x86_64-linux.deviceTreeCompiler = let
      src = dtc;
    in
    nixpkgs.stdenv.mkDerivation {
      name = "deviceTreeCompiler-1.8.0";
      
      buildInputs = [
        nixpkgs.gnumake
        nixpkgs.gcc
        nixpkgs.flex
        nixpkgs.bison
        nixpkgs.pkgconfig
      ];

      buildPhase = ''
        make dtc
      '';

      installPhase = ''
        mkdir -p $out/bin
        cp dtc $out/bin/dtc
      '';

      checkPhase = ''
        make tests
        $out/bin/dtc --version
      '';

      meta = with nixpkgs.lib; {
        description = "DeviceTree Compiler v1.8.0 - Fork by MtM";
        license = licenses.gpl2;
        maintainers = [ "Maxine Alexander <max.alexander3721@gmail.com>" ];
        platforms = platforms.linux;
      };
    };

    packages.x86_64-linux.default = self.packages.x86_64-linux.dtc;
  };
}
