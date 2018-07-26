
CREATE TABLE [dbo].[lora_binary] (
	[id] [int] IDENTITY (1, 1) NOT NULL ,
	[bindata] [text] COLLATE Cyrillic_General_CI_AS NULL ,
	[dt] [datetime] NOT NULL 
) ON [PRIMARY] TEXTIMAGE_ON [PRIMARY]
GO

