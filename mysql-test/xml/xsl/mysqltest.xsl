<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:output method="text"/>

<xsl:template match="/"><xsl:apply-templates /></xsl:template>

<!-- Main Template -->

<xsl:template match="/test"># <xsl:apply-templates select="@name"/>
#
# Versions
# --------<xsl:apply-templates select="version"/>
#
# Description
# -----------<xsl:apply-templates select="description"/>
#

<xsl:apply-templates select="connect"/>

<xsl:apply-templates select="connection"/>

<xsl:apply-templates select="sql"/>

<xsl:apply-templates select="resultfile"/>

</xsl:template>

<!-- End Main Template -->


<xsl:template match="version">
#   <xsl:apply-templates select="@value"/>
</xsl:template>

<xsl:template match="description">
# <xsl:apply-templates />
</xsl:template>

<xsl:template match="connect">
connect(<xsl:apply-templates select="@name"/>, <xsl:apply-templates select="@host"/>, <xsl:apply-templates select="@user"/>, <xsl:apply-templates select="@pass"/>, <xsl:apply-templates select="@db"/>, <xsl:apply-templates select="@port"/>, <xsl:apply-templates select="@sock"/>)
</xsl:template>

<xsl:template match="connection">
<xsl:text>
connection </xsl:text><xsl:apply-templates select="@name"/>
<xsl:text>
</xsl:text>
<xsl:apply-templates select="sql"/>
<xsl:apply-templates select="resultfile"/>
</xsl:template>

<xsl:template match="resultfile">@<xsl:apply-templates select="@name"/><xsl:text> </xsl:text><xsl:apply-templates select="sql"/>
</xsl:template>

<xsl:template match="sql">
<xsl:apply-templates />;
</xsl:template>
</xsl:stylesheet>
