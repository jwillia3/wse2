/* vim: set noexpandtab:tabstop=8 */
enum {
	MoveUp,
	MoveDown,
	MoveLeft,
	MoveWordLeft,
	MoveRight,
	MoveWordRight,
	MoveHome,
	MoveEnd,
	MovePageDown,
	MovePageUp,
	MoveSof,
	MoveEof,
	MoveBrace,
	MoveOpen,
	MoveClose,
	ToggleOverwrite,

	DeleteChar,
	BackspaceChar,
	
	SpaceAbove,
	SpaceBelow,
	SpaceBoth,
	AlignDelimiters,
	
	DeleteLine,
	BreakLine,
	JoinLine,
	Duplicate,
	ClearLeft,
	ClearRight,
	AscendLine,
	DescendLine,
	
	SelectAll,
	StartSelection,
	EndSelection,
	SelectWord,
	DeleteSelection,
	IndentSelection,
	UnindentSelection,
	CommentSelection,
	
	UndoChange,
	RedoChange,
	
	CopySelection,
	CutSelection,
	PasteClipboard,
	
	PromptFind,
	PromptReplace,
	
	ExitEditor,
	SpawnEditor,
	SpawnShell,
	
	SpawnCmd,
	PromptSpawn,
	
	AddBookmark,
	DeleteBookmark,
	ToggleBookmark,
	PrevBookmark,
	NextBookmark,
	
	ReloadConfig,
	PrevConfig,
	NextConfig,
	EditConfig,
	
	ToggleTransparency,
	ToggleFullscreen,
	ToggleMinimap,
	
	NewTab,
	NextTab,
	PreviousTab,
	CloseTab,
	
	RaiseFontMagnification,
	LowerFontMagnification,
	ResetFontMagnification,
};
