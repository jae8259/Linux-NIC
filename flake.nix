{
  description = "Linux NIC Study / same-host latency fast-path lab";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };

        isLinux = pkgs.stdenv.hostPlatform.isLinux;

        commonPackages = with pkgs; [
          bash
          gnumake
          gcc
          python3
          pkg-config
          jq
        ];

        linuxPackages = with pkgs; [
          iproute2
          iputils
          ethtool
          perf-tools
          trace-cmd
        ];
      in
      {
        devShells.default = pkgs.mkShell {
          packages = commonPackages ++ pkgs.lib.optionals isLinux linuxPackages;

          shellHook = ''
            echo "Entering linux-nic-study dev shell (${system})"
            echo
            echo "Build:"
            echo "  make all"
            echo "  make test"
            echo
            if [ "$(uname -s)" = "Linux" ]; then
              echo "Linux runtime commands:"
              echo "  sudo env PATH=$PATH make bench-veth"
              echo "  make bench-shm"
              echo "  sudo env PATH=$PATH make matrix"
            else
              echo "Non-Linux host detected."
              echo "Use this shell for building/docs, but run netns/shm benchmarks in Ubuntu VM or Linux host."
            fi
          '';
        };
      });
}
