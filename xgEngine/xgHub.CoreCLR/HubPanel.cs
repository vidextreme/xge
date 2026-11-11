// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
using System.Collections.Concurrent;
using xg;
using xg.imui;
using xg.math;
using Process = System.Diagnostics.Process;
using ProcessStartInfo = System.Diagnostics.ProcessStartInfo;
using Stopwatch = System.Diagnostics.Stopwatch;

namespace xgHub.CoreCLR
{
    // Project Hub window: the project list (search, add, create, open, preview, remove) filling the main window.
    // Opening or creating a project starts a separate xge.exe (default config), then the hub closes.
    public sealed class HubPanel : Panel
    {
        private const float RowHeight = 84.0f;
        private const float ThumbWidth = 128.0f;
        private const float ThumbHeight = 72.0f;
        private const float RowButtonSize = 28.0f;
        private const float ScrollbarAllowance = 14.0f;
        private const float StatusHeight = 22.0f;
        private const float NewPopupWidth = 540.0f;
        private const float RemovePopupWidth = 420.0f;
        private const double RefreshInterval = 1.0;
        private const string NewPopupId = "hub_new";
        private const string RemovePopupId = "hub_remove";

        private static readonly Color32[] _placeholderColors =
        {
            new(64, 94, 140), new(120, 78, 140), new(58, 122, 104),
            new(150, 96, 58), new(140, 62, 76), new(86, 104, 70),
        };

        private readonly List<HubProject> _projects = new();
        private readonly List<HubProject> _visible = new();
        private readonly ConcurrentQueue<Action> _pending = new();
        private readonly TextBuffer _search = new(128);
        private readonly TextBuffer _newName = new(128, "NewGame");
        private readonly TextBuffer _newLocation = new(1024);
        private readonly Stopwatch _clock = Stopwatch.StartNew();
        private ListState _list;
        private HubProject? _selected;
        private HubProject? _pendingRemove;
        private bool _openNewPopup;
        private bool _openRemovePopup;
        private string _newError = string.Empty;
        private string _status = string.Empty;
        private bool _statusError;
        private double _nextRefresh = RefreshInterval;

        public HubPanel()
        {
            HubStore.Load();
            SyncProjects();
            _newLocation.Text = HubStore.Data.NewProjectLocation ?? Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "XGE Projects");
            Debug.Log($"[Hub] {_projects.Count} project(s) from {HubStore.FilePath}");
        }

        public override string Title => "Project Hub";
        public override PanelFlags Flags => PanelFlags.FillViewport | PanelFlags.NoScrollbar;

        protected override void OnGUI()
        {
            while (_pending.TryDequeue(out Action? action))
                action();
            if (_clock.Elapsed.TotalSeconds >= _nextRefresh)
            {
                _nextRefresh = _clock.Elapsed.TotalSeconds + RefreshInterval;
                foreach (HubProject project in _projects)
                    project.Refresh();
            }

            DrawHeader();
            Vector2 avail = Gui.AvailableSize;
            float listHeight = Math.Max(RowHeight, avail.y - StatusHeight);
            if (_projects.Count == 0)
                DrawEmptyState(new Vector2(0.0f, listHeight));
            else
                DrawProjects(new Vector2(0.0f, listHeight), avail.x);
            DrawStatus();

            if (_openNewPopup)
            {
                _openNewPopup = false;
                _newError = string.Empty;
                Gui.OpenPopup(NewPopupId);
            }
            if (_openRemovePopup)
            {
                _openRemovePopup = false;
                Gui.OpenPopup(RemovePopupId);
            }
            DrawNewProjectPopup();
            DrawRemovePopup();
        }

        protected override void OnDestroy()
        {
            foreach (HubProject project in _projects)
                project.Dispose();
            _projects.Clear();
            _visible.Clear();
        }

        // ------------------------------------------------------------
        // Layout
        // ------------------------------------------------------------

        private void DrawHeader()
        {
            using (Gui.Toolbar())
            {
                using (Gui.Font(FontStyle.Bold))
                    Gui.Label("Projects");
                Gui.SameLine(16.0f);
                Gui.SearchField("##hub_search", _search, 260.0f);

                float buttons = Gui.MeasureText("Add").x + Gui.MeasureText("New project").x + 20.0f;
                Gui.SameLine();
                Gui.AlignRight(buttons);
                if (Gui.Button("Add"))
                    BrowseAddProject();
                Gui.SameLine();
                if (Gui.Button("New project"))
                    _openNewPopup = true;
            }
        }

        private void DrawEmptyState(Vector2 size)
        {
            using CanvasScope canvas = Gui.Canvas("##hub_empty", size);
            Color32 dim = Style.Color(StyleColor.TextDim);
            const string line1 = "No projects yet";
            const string line2 = "Add an existing project folder, or create a new project.";
            float y = canvas.Size.y * 0.4f;
            using (Gui.Font(FontStyle.Bold))
                canvas.Draw.Text(new Vector2((canvas.Size.x - Gui.MeasureText(line1).x) * 0.5f, y), Style.Color(StyleColor.Text), line1);
            canvas.Draw.Text(new Vector2((canvas.Size.x - Gui.MeasureText(line2).x) * 0.5f, y + 24.0f), dim, line2);
        }

        private void DrawProjects(Vector2 size, float width)
        {
            RebuildVisible();
            _list.Count = _visible.Count;
            _list.RowHeight = RowHeight;
            using ListScope list = Gui.List("##hub_projects", ref _list, size);
            if (!list.IsOpen)
                return;

            Color32 text = Style.Color(StyleColor.Text);
            Color32 dim = Style.Color(StyleColor.TextDim);
            Color32 warning = Style.Color(StyleColor.IconWarning);
            float textX = 8.0f + ThumbWidth + 14.0f;
            float rowWidth = width - ScrollbarAllowance;
            for (int i = _list.First; i < _list.Last && i < _visible.Count; ++i)
            {
                HubProject project = _visible[i];
                bool selected = project == _selected;
                using ListRowScope row = Gui.ListRow(i, selected);
                if (row.Clicked)
                    _selected = project;

                project.UpdateThumbnail();
                Painter draw = row.Draw;
                Rect thumb = Rect.FromXYWH(8.0f, (RowHeight - ThumbHeight) * 0.5f, ThumbWidth, ThumbHeight);
                Result preview = Gui.HitArea($"##hub_preview{i}", thumb);
                bool previewHovered = (preview & Result.Hovered) != 0;
                if (previewHovered && !project.Missing)
                    Gui.Tooltip("Open project");
                DrawThumbnail(draw, project, thumb, previewHovered && !project.Missing);
                if ((preview & Result.Clicked) != 0)
                {
                    _selected = project;
                    if (!project.Missing)
                        OpenProject(project);
                }

                using (Gui.Font(FontStyle.Bold))
                    draw.Text(new Vector2(textX, 12.0f), project.Missing ? dim : text, project.Name);
                draw.Text(new Vector2(textX, 34.0f), dim, project.Path);
                if (project.Missing)
                    draw.Text(new Vector2(textX, 54.0f), warning, "Missing: the folder or its .xgproject file was not found");
                else
                    draw.Text(new Vector2(textX, 54.0f), dim, OpenedText(project.Entry.LastOpened));

                if (selected || (row.Interaction & Result.Hovered) != 0)
                    DrawRowButtons(project, i, rowWidth);
            }
        }

        private void DrawRowButtons(HubProject project, int index, float rowWidth)
        {
            Color32 icon = Style.Color(StyleColor.Text);
            float y = (RowHeight - RowButtonSize) * 0.5f;
            float x = rowWidth - 8.0f - RowButtonSize;
            if (Gui.IconButton($"##hub_remove{index}", Icon.Trash, new Vector2(x, y), RowButtonSize, icon))
            {
                _pendingRemove = project;
                _openRemovePopup = true;
            }
            Gui.Tooltip("Remove from list...");
            if (project.Missing)
                return;
            x -= RowButtonSize + 4.0f;
            if (Gui.IconButton($"##hub_preview_set{index}", Icon.Image, new Vector2(x, y), RowButtonSize, icon))
                BrowsePreview(project);
            Gui.Tooltip("Set preview...");
        }

        private static void DrawThumbnail(Painter draw, HubProject project, Rect rect, bool hovered)
        {
            Texture? thumbnail = project.Thumbnail;
            if (thumbnail is not null && thumbnail.IsValid && thumbnail.Width > 0 && thumbnail.Height > 0)
            {
                float imageAspect = thumbnail.Width / (float)thumbnail.Height;
                float rectAspect = rect.Width / rect.Height;
                Vector2 uv0 = new(0.0f, 0.0f);
                Vector2 uv1 = new(1.0f, 1.0f);
                if (imageAspect > rectAspect)
                {
                    float crop = (1.0f - rectAspect / imageAspect) * 0.5f;
                    uv0.x = crop;
                    uv1.x = 1.0f - crop;
                }
                else
                {
                    float crop = (1.0f - imageAspect / rectAspect) * 0.5f;
                    uv0.y = crop;
                    uv1.y = 1.0f - crop;
                }
                draw.Image(thumbnail, rect, uv0, uv1, project.Missing ? new Color32(255, 255, 255, 110) : new Color32(255, 255, 255));
            }
            else
            {
                Color32 fill = _placeholderColors[(int)((uint)StableHash(project.Name) % (uint)_placeholderColors.Length)];
                if (project.Missing)
                    fill = new Color32(70, 70, 70);
                draw.RectFilled(rect, fill, 4.0f);
                if (!hovered)
                {
                    string initials = Initials(project.Name);
                    using (Gui.Font(FontStyle.Bold))
                    {
                        Vector2 size = Gui.MeasureText(initials);
                        draw.Text(new Vector2(rect.min.x + (rect.Width - size.x) * 0.5f, rect.min.y + (rect.Height - size.y) * 0.5f),
                            new Color32(240, 240, 240), initials);
                    }
                }
            }
            if (hovered)
            {
                draw.RectFilled(rect, new Color32(0, 0, 0, 120), 4.0f);
                Vector2 center = new(rect.min.x + rect.Width * 0.5f, rect.min.y + rect.Height * 0.5f);
                draw.CircleFilled(center, 18.0f, new Color32(0, 0, 0, 140));
                draw.Icon(Icon.Play, center, 20.0f, new Color32(255, 255, 255));
            }
            draw.Rect(rect, Style.Color(StyleColor.Outline), 4.0f);
        }

        private void DrawStatus()
        {
            using CanvasScope canvas = Gui.Canvas("##hub_status", new Vector2(0.0f, StatusHeight));
            string text = _status.Length > 0 ? _status : $"{_projects.Count} project(s)  |  {HubStore.FilePath}";
            Color32 color = _statusError ? Style.Color(StyleColor.IconMaterial) : Style.Color(StyleColor.TextDim);
            canvas.Draw.Text(new Vector2(4.0f, (StatusHeight - Gui.MeasureText("A").y) * 0.5f), color, text);
        }

        private void DrawNewProjectPopup()
        {
            using ModalScope modal = Gui.Modal(NewPopupId, "New Project", new Vector2(NewPopupWidth, 0.0f));
            if (!modal.IsOpen)
                return;

            float width = NewPopupWidth - 28.0f;
            Gui.Label("Name");
            Gui.TextField("##hub_new_name", _newName, "Project name", width);
            Gui.Label("Location");
            float browse = Gui.MeasureText("Browse...").x + 16.0f;
            Gui.TextField("##hub_new_location", _newLocation, "Parent folder", width - browse - 8.0f);
            Gui.SameLine();
            if (Gui.Button("Browse..."))
                BrowseNewLocation();

            string? target = NewProjectTarget(out _);
            using (CanvasScope info = Gui.Canvas("##hub_new_info", new Vector2(width, 40.0f)))
            {
                info.Draw.Text(new Vector2(0.0f, 2.0f), Style.Color(StyleColor.TextDim),
                    target is null ? "Enter a name and a parent folder" : $"Creates {target}");
                if (_newError.Length > 0)
                    info.Draw.Text(new Vector2(0.0f, 22.0f), Style.Color(StyleColor.IconMaterial), _newError);
            }

            float buttons = Gui.MeasureText("Create").x + Gui.MeasureText("Cancel").x + 24.0f;
            Gui.AlignRight(buttons);
            if (Gui.Button("Create") && CreateProject())
                Gui.CloseModal();
            Gui.SameLine();
            if (Gui.Button("Cancel"))
                Gui.CloseModal();
        }

        private void DrawRemovePopup()
        {
            using ModalScope modal = Gui.Modal(RemovePopupId, "Remove Project", new Vector2(RemovePopupWidth, 0.0f));
            if (!modal.IsOpen)
                return;
            HubProject? project = _pendingRemove;
            if (project is null)
            {
                Gui.CloseModal();
                return;
            }

            Gui.Label($"Remove \"{project.Name}\" from the list?");
            using (CanvasScope note = Gui.Canvas("##hub_remove_note", new Vector2(RemovePopupWidth - 28.0f, 26.0f)))
                note.Draw.Text(new Vector2(0.0f, 2.0f), Style.Color(StyleColor.TextDim), "The project folder and its files are not deleted.");

            float buttons = Gui.MeasureText("Remove").x + Gui.MeasureText("Cancel").x + 24.0f;
            Gui.AlignRight(buttons);
            if (Gui.Button("Remove"))
            {
                RemoveProject(project);
                _pendingRemove = null;
                Gui.CloseModal();
            }
            Gui.SameLine();
            if (Gui.Button("Cancel"))
            {
                _pendingRemove = null;
                Gui.CloseModal();
            }
        }

        // ------------------------------------------------------------
        // Actions
        // ------------------------------------------------------------

        private void BrowseAddProject()
        {
            FileDialog.OpenFolder(path => _pending.Enqueue(() => AddProject(path)), _newLocation.Text);
        }

        private void BrowseNewLocation()
        {
            FileDialog.OpenFolder(path => _pending.Enqueue(() =>
            {
                if (path is not null)
                    _newLocation.Text = path;
            }), _newLocation.Text);
        }

        private void BrowsePreview(HubProject project)
        {
            FileDialog.OpenFile(path => _pending.Enqueue(() => SetPreview(project, path)), "PNG image", "png", project.Path);
        }

        private void AddProject(string? path)
        {
            if (string.IsNullOrEmpty(path))
                return;
            bool hasManifest = false;
            try
            {
                hasManifest = Directory.Exists(path) && Directory.EnumerateFiles(path, "*" + HubProject.ManifestExtension).Any();
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
            }
            if (!hasManifest)
            {
                SetStatus($"{path} is not an XGE project (no {HubProject.ManifestExtension} file)", true);
                return;
            }
            HubProjectEntry entry = HubStore.Add(path);
            SyncProjects();
            _selected = _projects.FirstOrDefault(p => p.Entry == entry);
            SetStatus($"Added {_selected?.Name ?? path}", false);
        }

        private void OpenProject(HubProject project)
        {
            if (!StartXge("--project", project.Path))
                return;
            HubStore.Touch(project.Path);
            SetStatus($"Opening {project.Name}...", false);
            ScriptRuntime.RequestQuit();
        }

        // Returns false (with _newError set) when the project cannot be created.
        private bool CreateProject()
        {
            _newError = string.Empty;
            string? target = NewProjectTarget(out string? error);
            if (target is null)
            {
                _newError = error ?? "Enter a name and a parent folder";
                return false;
            }
            try
            {
                if (Directory.Exists(target) && Directory.EnumerateFileSystemEntries(target).Any())
                {
                    _newError = "That folder already exists and is not empty";
                    return false;
                }
                Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                _newError = $"Cannot use this location: {e.Message}";
                return false;
            }
            if (!StartXge("--new-project", target))
            {
                _newError = _status;
                return false;
            }

            HubStore.Data.NewProjectLocation = _newLocation.Text.Trim();
            HubStore.Touch(target);
            SyncProjects();
            _selected = _projects.FirstOrDefault(p => HubStore.SamePath(p.Path, target));
            SetStatus($"Creating {target}...", false);
            ScriptRuntime.RequestQuit();
            return true;
        }

        private void SetPreview(HubProject project, string? source)
        {
            if (string.IsNullOrEmpty(source))
                return;
            try
            {
                string destination = project.ThumbnailPath;
                Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
                if (!HubStore.SamePath(source, destination))
                    File.Copy(source, destination, true);
                project.InvalidateThumbnail();
                project.Refresh();
                SetStatus($"Preview for {project.Name} set from {source}", false);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                SetStatus($"Cannot set the preview: {e.Message}", true);
            }
        }

        private void RemoveProject(HubProject project)
        {
            HubStore.Remove(project.Path);
            if (_selected == project)
                _selected = null;
            SyncProjects();
            SetStatus($"Removed {project.Name} from the list (files are untouched)", false);
        }

        private bool StartXge(string flag, string path)
        {
            string? exe = Environment.ProcessPath;
            if (string.IsNullOrEmpty(exe) || !File.Exists(exe))
            {
                SetStatus("Cannot find xge.exe", true);
                return false;
            }
            try
            {
                ProcessStartInfo info = new(exe, $"{flag} \"{path.TrimEnd('\\')}\"")
                {
                    UseShellExecute = true,
                    WorkingDirectory = Path.GetDirectoryName(exe)!,
                };
                using Process? process = Process.Start(info);
                Debug.Log($"[Hub] Started xge.exe {flag} \"{path}\" (pid {process?.Id})");
                return process is not null;
            }
            catch (Exception e) when (e is System.ComponentModel.Win32Exception or InvalidOperationException)
            {
                SetStatus($"Cannot start xge.exe: {e.Message}", true);
                return false;
            }
        }

        // ------------------------------------------------------------
        // Helpers
        // ------------------------------------------------------------

        private void SyncProjects()
        {
            List<HubProject> previous = new(_projects);
            _projects.Clear();
            foreach (HubProjectEntry entry in HubStore.Data.Projects)
            {
                HubProject? existing = previous.FirstOrDefault(p => p.Entry == entry);
                if (existing is not null)
                    previous.Remove(existing);
                _projects.Add(existing ?? new HubProject(entry));
            }
            foreach (HubProject removed in previous)
                removed.Dispose();
        }

        private void RebuildVisible()
        {
            _visible.Clear();
            string filter = _search.Text.Trim();
            foreach (HubProject project in _projects)
            {
                if (filter.Length == 0 || project.Name.Contains(filter, StringComparison.OrdinalIgnoreCase)
                    || project.Path.Contains(filter, StringComparison.OrdinalIgnoreCase))
                    _visible.Add(project);
            }
            _visible.Sort((a, b) =>
            {
                int byTime = Nullable.Compare(b.Entry.LastOpened, a.Entry.LastOpened);
                return byTime != 0 ? byTime : string.Compare(a.Name, b.Name, StringComparison.OrdinalIgnoreCase);
            });
        }

        private string? NewProjectTarget(out string? error)
        {
            error = null;
            string name = _newName.Text.Trim();
            string location = _newLocation.Text.Trim();
            if (name.Length == 0 || location.Length == 0)
                return null;
            if (!Path.IsPathFullyQualified(location))
            {
                error = "The location must be a full path";
                return null;
            }
            return Path.Combine(location, ToIdentifier(name));
        }

        private void SetStatus(string text, bool error)
        {
            _status = text;
            _statusError = error;
            if (error)
                Debug.LogWarning($"[Hub] {text}");
            else
                Debug.Log($"[Hub] {text}");
        }

        private static string OpenedText(DateTime? lastOpened)
        {
            if (lastOpened is null)
                return "Never opened";
            TimeSpan age = DateTime.UtcNow - lastOpened.Value;
            if (age.TotalMinutes < 1.0)
                return "Opened just now";
            if (age.TotalHours < 1.0)
                return $"Opened {(int)age.TotalMinutes} minute(s) ago";
            if (age.TotalDays < 1.0)
                return $"Opened {(int)age.TotalHours} hour(s) ago";
            if (age.TotalDays < 30.0)
                return $"Opened {(int)age.TotalDays} day(s) ago";
            return $"Opened {lastOpened.Value.ToLocalTime():d MMM yyyy}";
        }

        private static string Initials(string name)
        {
            char first = '\0';
            char second = '\0';
            for (int i = 0; i < name.Length; ++i)
            {
                char c = name[i];
                if (!char.IsLetterOrDigit(c))
                    continue;
                bool wordStart = i == 0 || !char.IsLetterOrDigit(name[i - 1]) || (char.IsUpper(c) && char.IsLower(name[i - 1]));
                if (first == '\0')
                    first = char.ToUpperInvariant(c);
                else if (wordStart)
                {
                    second = char.ToUpperInvariant(c);
                    break;
                }
            }
            if (first == '\0')
                return "?";
            return second == '\0' ? first.ToString() : $"{first}{second}";
        }

        private static int StableHash(string text)
        {
            unchecked
            {
                int hash = (int)2166136261;
                foreach (char c in text)
                    hash = (hash ^ c) * 16777619;
                return hash;
            }
        }

        // Same rule as the editor's ProjectInfo.ToIdentifier: the folder name becomes the project's C# identifier.
        private static string ToIdentifier(string name)
        {
            Span<char> buffer = stackalloc char[Math.Max(1, name.Length)];
            int length = 0;
            foreach (char c in name)
            {
                if (char.IsLetterOrDigit(c) || c == '_')
                    buffer[length++] = c;
            }
            if (length == 0)
                return "Game";
            string result = new(buffer[..length]);
            return char.IsDigit(result[0]) ? "_" + result : result;
        }
    }
}
