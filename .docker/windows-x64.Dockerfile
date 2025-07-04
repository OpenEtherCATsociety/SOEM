FROM dockcross/windows-static-x64
ENV DEFAULT_DOCKCROSS_IMAGE=soem-windows-x64

# Preinstall uv in our image
COPY --from=ghcr.io/astral-sh/uv:0.5.9 /uv /uvx /usr/local/bin/
ENV UV_LINK_MODE=copy

# Prepare dockcross from use in jenkins
ARG BUILDER_UID=1001
ARG BUILDER_GID=1001
ARG BUILDER_USER=rtljenkins
ARG BUILDER_GROUP=rtljenkins
RUN BUILDER_UID=${BUILDER_GID} BUILDER_GID=${BUILDER_GID} BUILDER_USER=${BUILDER_USER} BUILDER_GROUP=${BUILDER_GROUP} /dockcross/entrypoint.sh echo "Setup jenkins user"
