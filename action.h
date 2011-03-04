enum {
	LoadFile,
	ReloadFile,
	ReloadFileUTF8,
	ReloadFileUTF16,
	SaveFile,
	SaveFileUTF8,
	SaveFileUTF16,

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

	DeleteChar,
	BackspaceChar,
	
	SpaceAbove,
	SpaceBelow,
	SpaceBoth,
	
	DeleteLine,
	BreakLine,
	JoinLine,
	DupLine,
	AscendLine,
	DescendLine,
	
	SelectAll,
	StartSelection,
	EndSelection,
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
	
	NewFile,
	QuitApp,
	ForkApp,
	SpawnApp,
	
	SpawnSelection,
	SpawnLastCmd,
	PromptSpawn,
	CaptureSpawn,
	
	ReloadConfig,
	PrevConfig,
	NextConfig,
	EditConfig
};








