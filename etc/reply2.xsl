<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
<xsl:output method="text"/>
<xsl:template match="/reply">
       <xsl:value-of select="status"/>
       <xsl:text>&#xA;</xsl:text>

       <xsl:choose>
       <xsl:when test="message/moduleinfo/*">
		<xsl:apply-templates select="message/moduleinfo"/>
       </xsl:when>
       <xsl:when test="message/meterinfos/*">
		<xsl:apply-templates select="message/meterinfos"/>
       </xsl:when>
       <xsl:otherwise> 
       		<xsl:apply-templates select="message"/>
       </xsl:otherwise>
       </xsl:choose>
</xsl:template>

<!-- template for module info -->
<xsl:template match="message/moduleinfo">
Module: <xsl:value-of select="module/name"/>
(version <xsl:value-of select="module/version"/>, uid=<xsl:value-of select="module/uid"/>)

Creation Date: <xsl:text>&#x9;</xsl:text><xsl:value-of select="module/created"/>
Last Modified: <xsl:text>&#x9;</xsl:text><xsl:value-of select="module/modified"/>
<xsl:text>&#xA;</xsl:text>
Description:

Short<xsl:text>&#x9;&#x9;</xsl:text><xsl:value-of select="module/brief"/>
Verbose<xsl:text>&#x9;&#x9;</xsl:text><xsl:value-of select="module/verbose"/>
documented at<xsl:text>&#x9;</xsl:text><xsl:value-of select="module/htmldoc"/>
Rule paramaters<xsl:text>&#x9;</xsl:text><xsl:value-of select="module/parameters"/>
Exported Data<xsl:text>&#x9;</xsl:text><xsl:value-of select="module/results"/>
<xsl:text>&#xA;</xsl:text>
Author:

Name<xsl:text>&#x9;</xsl:text><xsl:value-of select="author/name"/>
Affiliation<xsl:text>&#x9;</xsl:text><xsl:value-of select="author/affiliation"/>
E-Mail<xsl:text>&#x9;&#x9;</xsl:text><xsl:value-of select="author/email"/>
Homepage<xsl:text>&#x9;</xsl:text><xsl:value-of select="author/homepage"/>
<xsl:text>&#xA;</xsl:text>
</xsl:template>

<!-- template for meter infos -->
<xsl:template match="message/meterinfos">

<xsl:for-each select="//info">
	<xsl:value-of select="@name"/>
	<xsl:text>&#x9;&#x9;</xsl:text>	
	<xsl:value-of select="."/>
	<xsl:text>&#xA;</xsl:text>	
</xsl:for-each>

</xsl:template>


<!-- template for everything else -->
<xsl:template match="message">
      <xsl:value-of select="."/>
      <xsl:text>&#xA;</xsl:text>
</xsl:template>

</xsl:stylesheet>
