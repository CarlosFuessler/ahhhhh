const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = if (@hasField(std.Build.ExecutableOptions, "root_module")) blk: {
        const exe_mod = b.createModule(.{
            .target = target,
            .optimize = optimize,
        });
        exe_mod.addIncludePath(b.path("src"));

        break :blk b.addExecutable(.{
            .name = "ahhhhh",
            .root_module = exe_mod,
        });
    } else blk: {
        break :blk b.addExecutable(.{
            .name = "ahhhhh",
            .root_source_file = b.path("src/main.c"),
            .target = target,
            .optimize = optimize,
        });
    };
    exe.linkLibC();
    exe.addIncludePath(b.path("src"));

    const use_raylib = b.option(bool, "raylib", "Enable Raylib support") orelse true;

    if (use_raylib) {
        const target_os = target.result.os.tag;
        if (target_os == .macos) {
            exe.addIncludePath(.{ .cwd_relative = "/opt/homebrew/include" });
            exe.addLibraryPath(.{ .cwd_relative = "/opt/homebrew/lib" });
            exe.linkFramework("CoreVideo");
            exe.linkFramework("IOKit");
            exe.linkFramework("Cocoa");
            exe.linkFramework("GLUT");
            exe.linkFramework("OpenGL");
        } else if (target_os == .linux) {
            exe.linkSystemLibrary("m");
            exe.linkSystemLibrary("pthread");
            exe.linkSystemLibrary("dl");
            exe.linkSystemLibrary("rt");
            exe.linkSystemLibrary("X11");
        } else if (target_os == .windows) {
            exe.linkSystemLibrary("gdi32");
            exe.linkSystemLibrary("winmm");
            exe.linkSystemLibrary("opengl32");
            exe.linkSystemLibrary("shell32");
        }
        exe.linkSystemLibrary("raylib");
    } else {
        const target_os = target.result.os.tag;
        if (target_os == .linux) {
            exe.linkSystemLibrary("m");
            exe.linkSystemLibrary("pthread");
        }
    }
    
    const sources = [_][]const u8{
        "src/main.c",
        "src/buffer/buffer.c",
        "src/lexer/token.c",
        "src/lexer/lexer.c",
        "src/lexer/token_stream.c",
        "src/parser/parser.c",
        "src/compiler/compiler.c",
        "src/compiler/type.c",
        "src/vm/chunk.c",
        "src/vm/table.c",
        "src/vm/stdlib.c",
        "src/vm/vm.c",
    };

    const c_flags = if (use_raylib)
        &[_][]const u8{ "-std=c99", "-DAHHHHH_HAS_RAYLIB=1" }
    else
        &[_][]const u8{ "-std=c99", "-DAHHHHH_HAS_RAYLIB=0" };

    exe.addCSourceFiles(.{ .files = &sources, .flags = c_flags });

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
