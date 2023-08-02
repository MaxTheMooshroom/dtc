{
  description = "DeviceTree Compiler v1.8.0 - Fork by MtM";

  inputs = {
    # what nixos version is this?
    nixpkgs.url = "github:nixos/nixpkgs/nixos-23.05";

    # dtc.url = "github:MaxTheMooshroom/dtc/dev";
  };

  outputs = { self, nixpkgs }: {
    packages.x86_64-linux.deviceTreeCompiler = let
      pkgs = nixpkgs.legacyPackages.x86_64-linux;
    in
    pkgs.stdenv.mkDerivation {
      name = "deviceTreeCompiler-1.8.0";
      
      src = ./.;

      buildInputs = [
        pkgs.gnumake
        pkgs.gcc
        pkgs.flex
        pkgs.bison
        pkgs.pkgconfig
      ];

      buildPhase = ''
        make dtc
      '';

      installPhase = ''
        mkdir -p $out/bin
        cp dtc $out/bin/dtc
      ''; # try doing something maybe?

      checkPhase = ''
        make tests
        $out/bin/dtc --version
      '';

      meta = with pkgs.lib; {
        description = "DeviceTree Compiler v1.8.0 - Fork by MtM";
        license = licenses.gpl2;
        maintainers = [ "Maxine Alexander <max.alexander3721@gmail.com>" ];
        platforms = platforms.linux;
      };
    };

    packages.x86_64-linux.default = self.packages.x86_64-linux.deviceTreeCompiler;
  };
}
