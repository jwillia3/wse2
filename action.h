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
	ToggleOverwrite,

	DeleteChar,
	BackspaceChar,
	
	SpaceAbove,
	SpaceBelow,
	SpaceBoth,
	
	DeleteLine,
	DeleteBraces,
	BreakLine,
	JoinLine,
	DupLine,
	AscendLine,
	DescendLine,
	PromptWrap,
	WrapLine,
	
	SelectAll,
	StartSelection,
	EndSelection,
	SelectWord,
	SelectBraces,
	DeleteSelection,
	IndentSelection,
	UnindentSelection,
	CommentSelection,
	
	UndoChange,
	RedoChange,
	
	CopySelection,
	CutSelection,
	PasteClipboard,
	
	PromptGo,
	PromptFind,
	PromptReplace,
	PromptOpen,
	PromptSaveAs,
	
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
	
	NewTab,
	NextTab,
	PreviousTab,
	CloseTab,
	
	RaiseFontMagnification,
	LowerFontMagnification,
	ResetFontMagnification,
};
