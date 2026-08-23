FROM debian:bookworm-slim@sha256:abd67ffcfa541b485a3dff59865ab629aa048a6c613e639d36e7456b0b229241

ARG TARGETARCH=amd64
ARG OSS_CAD_RELEASE=2026-08-23
ARG OSS_CAD_RELEASE_DATE=20260823

LABEL org.opencontainers.image.title="VERA OSS CAD toolchain"
LABEL org.opencontainers.image.description="Pinned OSS CAD Suite for building the VERA iCE40UP5K FPGA design"
LABEL org.opencontainers.image.source="https://github.com/YosysHQ/oss-cad-suite-build"
LABEL org.opencontainers.image.version="${OSS_CAD_RELEASE}"

RUN set -eux; \
    case "${TARGETARCH}" in \
        amd64) \
            oss_cad_arch="linux-x64"; \
            oss_cad_sha256="063d7b4f5663271cf04529ba22266e21baa9e3431b236fe71e1bf589d6d8816a" \
            ;; \
        arm64) \
            oss_cad_arch="linux-arm64"; \
            oss_cad_sha256="c77dfc2a865d2de8d298d7d2a34879424a9df7687009f1f4e59978ef4df61117" \
            ;; \
        *) \
            echo "Unsupported Docker architecture: ${TARGETARCH}" >&2; \
            exit 1 \
            ;; \
    esac; \
    archive="oss-cad-suite-${oss_cad_arch}-${OSS_CAD_RELEASE_DATE}.tgz"; \
    url="https://github.com/YosysHQ/oss-cad-suite-build/releases/download/${OSS_CAD_RELEASE}/${archive}"; \
    apt-get update; \
    apt-get install --yes --no-install-recommends ca-certificates curl; \
    curl --fail --location --retry 5 --output "/tmp/${archive}" "${url}"; \
    echo "${oss_cad_sha256}  /tmp/${archive}" | sha256sum --check --strict; \
    tar --extract --gzip --file "/tmp/${archive}" --directory /opt; \
    rm "/tmp/${archive}"; \
    apt-get purge --yes --auto-remove curl; \
    rm -rf /var/lib/apt/lists/*

ENV PATH="/opt/oss-cad-suite/bin:${PATH}"
ENV HOME="/tmp"

WORKDIR /workspace

RUN yosys --version \
    && nextpnr-ice40 --version \
    && iverilog -V \
    && rm -rf /tmp/.config

CMD ["bash"]
