#!/bin/sh
set -e

# Toolchain can either be GCCSDK or ARCHIESDK
export TOOLCHAIN=ARCHIESDK
# Set this to your hostfs path
# e.g., export TARGETCOPY=/mnt/c/dev/ACORN/Arculator_V2.1_Windows/hostfs
export TARGETCOPY=./output/to/hostfs

# Build options
export USE_256_COLORS=yes
export TARGET_A5000=no

# --all builds every colour/machine combination; --adfs also packages disk images.
variants="$USE_256_COLORS:$TARGET_A5000"
adfs=no
for option do
    case "$option" in
        --all) variants="no:no yes:no no:yes yes:yes" ;;
        --adfs) adfs=yes ;;
        *) echo "Usage: $0 [--all] [--adfs]" >&2; exit 1 ;;
    esac
done

if [ "$adfs" = yes ]; then
    if [ -x .venv/bin/disc ]; then PATH="$PWD/.venv/bin:$PATH"; fi
    if ! command -v disc >/dev/null 2>&1; then
        echo "ADF packaging requires oaknut-disc; see README.md for build setup." >&2
        exit 1
    fi
    mkdir -p Images
fi

cd src
# Variants share object files and the output binary, so build and copy in sequence.
for variant in $variants; do
    export USE_256_COLORS="${variant%:*}"
    export TARGET_A5000="${variant#*:}"
    echo "Building USE_256_COLORS=$USE_256_COLORS TARGET_A5000=$TARGET_A5000"
    if [ "$TOOLCHAIN" = "GCCSDK" ]; then
        echo "Using GCCSDK toolchain"
        make clean
        make -j4 USE_256_COLORS=$USE_256_COLORS TARGET_A5000=$TARGET_A5000

        # Uncomment RM lines in !Run,feb for GCCSDK builds (in case they were commented by ARCHIESDK)
        sed -i.bak 's/^| RMLoad /RMLoad /' ./!Snowscape/!Run,feb
        sed -i.bak 's/^| RMEnsure /RMEnsure /' ./!Snowscape/!Run,feb
        rm -f ./!Snowscape/!Run,feb.bak
    elif [ "$TOOLCHAIN" = "ARCHIESDK" ]; then
        echo "Using ARCHIESDK toolchain"
        make -f Makefile clean
        make -f Makefile -j4 USE_256_COLORS=$USE_256_COLORS TARGET_A5000=$TARGET_A5000

        # Comment out RM lines in !Run,feb for ARCHIESDK builds
        sed -i.bak 's/^RMLoad /| RMLoad /' ./!Snowscape/!Run,feb
        sed -i.bak 's/^RMEnsure /| RMEnsure /' ./!Snowscape/!Run,feb
        rm -f ./!Snowscape/!Run,feb.bak
    else
        echo "Unknown TOOLCHAIN: $TOOLCHAIN"
        exit 1
    fi

    # Copy each build variant to its own app folder in hostfs
    APPNAME='!Snow16'
    if [ "$USE_256_COLORS" = "yes" ]; then APPNAME='!Snow256'; fi
    if [ "$TARGET_A5000" = "yes" ]; then APPNAME="${APPNAME}A5k"; fi
    if [ "$adfs" = yes ]; then
        stage=$(mktemp -d ../Images/.build.XXXXXX)
    else
        stage=$(mktemp -d)
    fi
    trap 'rm -rf "$stage"' 0
    trap 'exit 1' HUP INT TERM
    # ADFS E names are limited to 10 characters; use the original app name on disc.
    mkdir -p "$stage/files/!Snowscape"
    for file in ./!Snowscape/*; do
        if [ "$TOOLCHAIN" = "ARCHIESDK" ]; then
            case "${file##*/}" in
                CallASWI,ffa|CLib,ffa|SharedULib,ffa) continue ;;
            esac
        fi
        cp -rf "$file" "$stage/files/!Snowscape/"
    done
    if [ "$adfs" = yes ]; then
        disc create "$stage/variant.adf" --geometry e --title "${APPNAME#!}"
        disc import "$stage/variant.adf" "$stage/files" --meta-format filename-riscos
        # Filename metadata has no access bits; imported files otherwise get no read permission.
        disc chmod -r "$stage/variant.adf:$.!Snowscape" WR/R
        disc validate "$stage/variant.adf"
        mv "$stage/variant.adf" "../Images/${APPNAME#!}.adf"
    fi
    mkdir -p "${TARGETCOPY}/${APPNAME}"
    cp -rf "$stage/files/!Snowscape/." "${TARGETCOPY}/${APPNAME}/"
    rm -rf "$stage"
done
