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
            .name = "ahhhh",
            .root_module = exe_mod,
        });
    } else blk: {
        break :blk b.addExecutable(.{
            .name = "ahhhh",
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
        "src/parser/parser_expr.c",
        "src/parser/parser_utils.c",
        "src/compiler/compiler.c",
        "src/compiler/type.c",
        "src/vm/chunk.c",
        "src/vm/table.c",
        "src/vm/stdlib.c",
        "src/vm/vm.c",
    };

    writeCompileCommands(b, &sources, use_raylib);

    const c_flags = if (use_raylib)
        &[_][]const u8{ "-std=gnu99", "-DAHHHHH_HAS_RAYLIB=1" }
    else
        &[_][]const u8{ "-std=gnu99", "-DAHHHHH_HAS_RAYLIB=0" };

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

fn writeCompileCommands(b: *std.Build, sources: []const []const u8, use_raylib: bool) void {
    const cwd_path = std.fs.cwd().realpathAlloc(b.allocator, ".") catch return;
    defer b.allocator.free(cwd_path);

    const file = std.fs.cwd().createFile("compile_commands.json", .{}) catch return;
    defer file.close();

    file.writeAll("[\n") catch return;

    const raylib_flag = if (use_raylib) "-DAHHHHH_HAS_RAYLIB=1" else "-DAHHHHH_HAS_RAYLIB=0";

    for (sources, 0..) |src, i| {
        file.writeAll("  {\n") catch return;
        
        // Escape paths for JSON
        const dir_line = std.fmt.allocPrint(b.allocator, "    \"directory\": \"{s}\",\n", .{cwd_path}) catch return;
        defer b.allocator.free(dir_line);
        file.writeAll(dir_line) catch return;

        const file_line = std.fmt.allocPrint(b.allocator, "    \"file\": \"{s}\",\n", .{src}) catch return;
        defer b.allocator.free(file_line);
        file.writeAll(file_line) catch return;
        
        // Command to run
        const cmd_line = std.fmt.allocPrint(b.allocator, "    \"command\": \"clang -std=gnu99 {s} -I{s}/src -I/opt/homebrew/include -c {s}/{s}\"\n", .{
            raylib_flag, cwd_path, cwd_path, src
        }) catch return;
        defer b.allocator.free(cmd_line);
        file.writeAll(cmd_line) catch return;

        if (i < sources.len - 1) {
            file.writeAll("  },\n") catch return;
        } else {
            file.writeAll("  }\n") catch return;
        }
    }

    file.writeAll("]\n") catch return;
}

